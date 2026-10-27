// Explicit roots repair omissions in the analysis database, never executable bytes.
// @category ExtendedPrecache

import com.google.gson.Gson;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

public class InspectAuditFunctions extends GhidraScript {
    private static final int schemaVersion = 1;

    private record Request(int schemaVersion, String expectedSha256, boolean recoverRoots,
                           List<String> roots) {}

    private record Observation(String requestedAddress, String functionEntry, String name,
                               boolean existedBefore, boolean noReturn, String status,
                               String diagnostic) {}

    @Override
    public void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2) {
            throw new IllegalArgumentException("Expected request JSON and a new output directory");
        }
        Gson gson = new Gson();
        Request request;
        try (var reader = Files.newBufferedReader(Path.of(arguments[0]), StandardCharsets.UTF_8)) {
            request = gson.fromJson(reader, Request.class);
        }
        if (request == null || request.schemaVersion() != schemaVersion || request.roots() == null ||
            !currentProgram.getExecutableSHA256().equalsIgnoreCase(request.expectedSha256())) {
            throw new IllegalArgumentException("Request schema or artifact identity mismatch");
        }
        Path directory = Path.of(arguments[1]).toAbsolutePath().normalize();
        if (Files.exists(directory)) {
            throw new IllegalArgumentException("Preserve previous observations: output directory already exists");
        }
        List<Boolean> existed = new ArrayList<>();
        var uniqueRoots = new HashSet<String>();
        for (String root : request.roots()) {
            monitor.checkCancelled();
            if (root == null || !root.matches("0x[0-9a-f]+") || !uniqueRoots.add(root)) {
                throw new IllegalArgumentException("Only explicit default-space hexadecimal addresses are accepted");
            }
            var address = toAddr(root);
            var block = currentProgram.getMemory().getBlock(address);
            if (block == null || !block.isInitialized() || !block.isExecute()) {
                throw new IllegalArgumentException("Root is outside initialized executable memory: " + root);
            }
        }
        Files.createDirectories(directory);
        for (String root : request.roots()) {
            monitor.checkCancelled();
            var address = toAddr(root);
            Function function = getFunctionAt(address);
            existed.add(function != null);
            if (function == null && request.recoverRoots() && getFunctionContaining(address) == null) {
                if (!disassemble(address)) {
                    throw new IllegalStateException("Cannot disassemble explicit root " + root);
                }
                createFunction(address, null);
            }
        }
        if (request.recoverRoots()) {
            analyzeChanges(currentProgram);
        }

        List<Observation> observations = new ArrayList<>();
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Cannot open decompiler: " + decompiler.getLastMessage());
        }
        try {
            for (int index = 0; index < request.roots().size(); index++) {
                monitor.checkCancelled();
                String root = request.roots().get(index);
                Function function = getFunctionAt(toAddr(root));
                if (function == null) {
                    observations.add(new Observation(root, null, null, existed.get(index), false,
                        "unresolved", "No exact function entry; no guessed containing-function substitution"));
                    continue;
                }
                var result = decompiler.decompileFunction(function, 0, monitor);
                String status = result.decompileCompleted() ? "decompiled" : "failed";
                observations.add(new Observation(root, "0x" + function.getEntryPoint(),
                    function.getName(), existed.get(index), function.hasNoReturn(), status,
                    result.getErrorMessage()));
                if (result.decompileCompleted()) {
                    Files.writeString(directory.resolve(root.substring(2) + ".c"),
                        result.getDecompiledFunction().getC(), StandardCharsets.UTF_8,
                        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
                }
            }
        } finally {
            decompiler.dispose();
        }
        try (var writer = Files.newBufferedWriter(directory.resolve("observations.json"),
            StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
            gson.toJson(observations, writer);
        }
        println("AUDIT_FUNCTION_INSPECTION_COMPLETE " + request.expectedSha256() + " " + observations.size());
    }
}
