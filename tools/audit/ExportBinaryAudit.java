// Exports static analysis observations without changing the imported program.
// @category ExtendedPrecache

import com.google.gson.Gson;
import ghidra.app.script.GhidraScript;
import ghidra.framework.Application;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.scalar.Scalar;

import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

public class ExportBinaryAudit extends GhidraScript {
    private record BlockObservation(String name, String start, String endInclusive, long bytes,
                                    boolean initialized, boolean read, boolean write, boolean execute) {}
    private sealed interface OperandObservation permits ScalarObservation, AddressObservation {}
    private record ScalarObservation(int operand, String kind, String value, int bits)
        implements OperandObservation {}
    private record AddressObservation(int operand, String kind, String value)
        implements OperandObservation {}
    private record InstructionObservation(String address, String function, int length, String text,
                                          List<OperandObservation> constants, String flowType,
                                          boolean computedFlow, List<String> flowDestinations,
                                          String fallThrough) {}
    private record ReferenceObservation(String from, String to, String function, String type,
                                        int operand, String source) {}
    private record BodyRange(String start, String endInclusive) {}
    private record FunctionObservation(String address, String name, boolean external, boolean thunk,
                                       boolean noReturn, List<BodyRange> ranges) {}
    private record IdentityObservation(int schemaVersion, String artifactName, String sha256,
                                       String ghidraVersion, String language, String compilerSpec,
                                       String imageBase, List<BlockObservation> blocks,
                                       long instructionCount, long functionCount, long referenceCount,
                                       long unownedInstructions, long computedFlowCount,
                                       long initializedExecutableBytes,
                                       long recognizedExecutableInstructionBytes,
                                       long definedExecutableDataBytes,
                                       long unclassifiedExecutableBytes, boolean analysisDatabaseChanged,
                                       String coverage) {}

    private static final int schemaVersion = 1;
    private final Gson gson = new Gson();

    private String addressText(Address address) {
        if (address == null) {
            return null;
        }
        if (address.getAddressSpace().equals(currentProgram.getAddressFactory().getDefaultAddressSpace())) {
            return "0x" + address;
        }
        return address.toString(true);
    }

    private BufferedWriter createWriter(Path directory, String name) throws IOException {
        return Files.newBufferedWriter(directory.resolve(name), StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
    }

    private void writeRow(BufferedWriter writer, Object value) throws IOException {
        gson.toJson(value, writer);
        writer.newLine();
    }

    private String functionAt(Address address) {
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        return function == null ? null : addressText(function.getEntryPoint());
    }

    @Override
    public void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2) {
            throw new IllegalArgumentException("Expected a new output directory and the exact input SHA-256");
        }
        Path directory = Path.of(arguments[0]).toAbsolutePath().normalize();
        String inputHash = currentProgram.getExecutableSHA256();
        if (!arguments[1].equalsIgnoreCase(inputHash)) {
            throw new IllegalArgumentException("Imported program SHA-256 does not match the selected artifact");
        }
        if (Files.exists(directory)) {
            throw new IllegalArgumentException("Output directory already exists; preserve the previous observation");
        }
        Files.createDirectories(directory);

        List<BlockObservation> blocks = new ArrayList<>();
        long executableBytes = 0;
        for (var block : currentProgram.getMemory().getBlocks()) {
            blocks.add(new BlockObservation(block.getName(), addressText(block.getStart()),
                addressText(block.getEnd()), block.getSize(), block.isInitialized(), block.isRead(),
                block.isWrite(), block.isExecute()));
            if (block.isExecute() && block.isInitialized()) {
                executableBytes += block.getSize();
            }
        }

        long instructionCount = 0;
        long instructionBytes = 0;
        long unownedInstructions = 0;
        long computedFlows = 0;
        try (var writer = createWriter(directory, "instructions.jsonl")) {
            var instructions = currentProgram.getListing().getInstructions(true);
            while (instructions.hasNext()) {
                monitor.checkCancelled();
                var instruction = instructions.next();
                String function = functionAt(instruction.getAddress());
                if (function == null) {
                    unownedInstructions++;
                }
                List<OperandObservation> constants = new ArrayList<>();
                for (int operandIndex = 0; operandIndex < instruction.getNumOperands(); operandIndex++) {
                    for (Object operand : instruction.getOpObjects(operandIndex)) {
                        if (operand instanceof Scalar scalar) {
                            constants.add(new ScalarObservation(operandIndex, "scalar",
                                "0x" + Long.toUnsignedString(scalar.getUnsignedValue(), 16),
                                scalar.bitLength()));
                        } else if (operand instanceof Address address) {
                            constants.add(new AddressObservation(operandIndex, "address", addressText(address)));
                        }
                    }
                }
                List<String> flows = new ArrayList<>();
                for (Address flow : instruction.getFlows()) {
                    flows.add(addressText(flow));
                }
                boolean computed = instruction.getFlowType().isComputed();
                if (computed) {
                    computedFlows++;
                }
                writeRow(writer, new InstructionObservation(addressText(instruction.getAddress()),
                    function, instruction.getLength(), instruction.toString(), constants,
                    instruction.getFlowType().toString(), computed, flows,
                    addressText(instruction.getFallThrough())));
                instructionCount++;
                var block = currentProgram.getMemory().getBlock(instruction.getAddress());
                if (block != null && block.isExecute() && block.isInitialized()) {
                    instructionBytes += instruction.getLength();
                }
            }
        }

        long referenceCount = 0;
        try (var writer = createWriter(directory, "references.jsonl")) {
            var references = currentProgram.getReferenceManager().getReferenceIterator(currentProgram.getMinAddress());
            while (references.hasNext()) {
                monitor.checkCancelled();
                var reference = references.next();
                writeRow(writer, new ReferenceObservation(addressText(reference.getFromAddress()),
                    addressText(reference.getToAddress()), functionAt(reference.getFromAddress()),
                    reference.getReferenceType().toString(), reference.getOperandIndex(),
                    reference.getSource().toString()));
                referenceCount++;
            }
        }

        long functionCount = 0;
        try (var writer = createWriter(directory, "functions.jsonl")) {
            var functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                var function = functions.next();
                List<BodyRange> ranges = new ArrayList<>();
                var bodyRanges = function.getBody().getAddressRanges();
                while (bodyRanges.hasNext()) {
                    var range = bodyRanges.next();
                    ranges.add(new BodyRange(addressText(range.getMinAddress()), addressText(range.getMaxAddress())));
                }
                writeRow(writer, new FunctionObservation(addressText(function.getEntryPoint()),
                    function.getName(), function.isExternal(), function.isThunk(), function.hasNoReturn(), ranges));
                functionCount++;
            }
        }

        long executableDataBytes = 0;
        var data = currentProgram.getListing().getDefinedData(true);
        while (data.hasNext()) {
            monitor.checkCancelled();
            var item = data.next();
            var block = currentProgram.getMemory().getBlock(item.getAddress());
            if (block != null && block.isExecute() && block.isInitialized()) {
                executableDataBytes += item.getLength();
            }
        }

        try (var writer = createWriter(directory, "identity.json")) {
            writeRow(writer, new IdentityObservation(schemaVersion, currentProgram.getName(), inputHash,
                Application.getApplicationVersion(), currentProgram.getLanguageID().toString(),
                currentProgram.getCompilerSpec().getCompilerSpecID().toString(),
                addressText(currentProgram.getImageBase()), blocks, instructionCount, functionCount,
                referenceCount, unownedInstructions, computedFlows, executableBytes, instructionBytes,
                executableDataBytes, executableBytes - instructionBytes - executableDataBytes,
                currentProgram.isChanged(),
                "Ghidra-defined instructions, functions and references after default auto-analysis; computed flows and unclassified bytes remain explicit"));
        }
        println("AUDIT_EXPORT_COMPLETE " + inputHash + " " + instructionCount + " instructions, "
            + referenceCount + " references, " + functionCount + " functions");
    }
}
