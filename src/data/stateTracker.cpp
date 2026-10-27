#include "stateIo.hpp"

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto flow(Route route) -> std::size_t {
    require(route.direction == Direction::serverToClient && validRoute(route));
    return route.delivery == Delivery::reliable ? 1U : 0U;
}
void frameRoute(Route route, FrameKind kind, const FrameBegin &begin) {
    require(route.kind == static_cast<std::uint8_t>(kind));
    const auto initial = route.opcode == static_cast<std::uint8_t>(Service::initialFrame);
    require(initial || route.opcode == static_cast<std::uint8_t>(Service::fullFrame) ||
            route.opcode == static_cast<std::uint8_t>(Service::deltaFrame));
    require((begin.base.value != 0) ==
            (route.opcode == static_cast<std::uint8_t>(Service::deltaFrame)));
}
void objectRoute(Route route, Route begin, ObjectKind kind) {
    require(route.delivery == begin.delivery);
    if (kind == ObjectKind::client || kind == ObjectKind::weapon) {
        require(route.opcode == static_cast<std::uint8_t>(Service::clientData) && route.kind == 2);
    } else if (kind == ObjectKind::module) {
        require(route.opcode == static_cast<std::uint8_t>(Service::resource) && route.kind == 6);
    } else {
        require(route.opcode == begin.opcode && route.kind == 2);
    }
}
} // namespace
auto FrameReceiver::failure(Failure error) noexcept -> std::unexpected<Failure> {
    failed = true;
    pending = {};
    return std::unexpected{error};
}
auto FrameReceiver::current() const noexcept -> std::shared_ptr<const StateSnapshot> {
    return retained.empty() ? nullptr : retained.back();
}
auto FrameReceiver::recoveryRequest() const noexcept -> std::optional<StateFrameId> {
    return !failed && recoveryNeeded && !requestQueued
               ? std::optional{current() ? current()->begin.id : StateFrameId{0}}
               : std::nullopt;
}
auto FrameReceiver::begin(Route route, FrameBegin frame) noexcept -> Result<FrameDisposition> {
    auto result = detail::protect([&] {
        require(!failed && windowFrames > 0, ErrorCode::wrongPhase);
        validateFrameBegin(frame);
        frameRoute(route, FrameKind::begin, frame);
        const auto index = flow(route);
        require(index == 0 || !pending[index], ErrorCode::wrongPhase);
        const auto prior = current();
        require(prior || route.opcode == static_cast<std::uint8_t>(Service::initialFrame),
                ErrorCode::wrongPhase);
        require(!prior || route.opcode != static_cast<std::uint8_t>(Service::initialFrame),
                ErrorCode::wrongPhase);
        within(frame.changeBytes.value, contextValue.frameBytes.value);
        const auto maxObjects =
            add(add(contextValue.objects.entityEntries, contextValue.objects.clientEntries),
                add(add(contextValue.objects.weaponEntries, contextValue.objects.staticEntries),
                    std::numeric_limits<std::uint16_t>::max()));
        within(frame.changeCount, maxObjects);
        require(multiply(frame.changeCount, frameObjectHeaderBytes) <= frame.changeBytes.value);
        Pending next{frame, route, {}, 0, prior && frame.id.value <= prior->begin.id.value, false};
        next.ownedBytes = multiply(frame.changeCount, sizeof(FrameObject));
        within(next.ownedBytes, contextValue.records.ownedBytes.value);
        if (!next.discard) {
            next.changes.reserve(frame.changeCount);
        }
        if (!next.discard && prior) {
            require(frame.serverTime >= prior->begin.serverTime);
        }
        if (frame.base.value != 0) {
            next.missingBase = std::ranges::none_of(
                retained, [&](const auto &snapshot) { return snapshot->begin.id == frame.base; });
            if (next.missingBase && !next.discard && index == 1) {
                fail(ErrorCode::sequence);
            }
        }
        if (pending[index] && !pending[index]->discard) {
            recoveryNeeded = true;
        }
        pending[index] = std::move(next);
        return pending[index]->discard ? FrameDisposition::obsolete : FrameDisposition::staged;
    });
    if (!result) {
        return failure(result.error());
    }
    return result;
}
auto FrameReceiver::object(Route route, const FrameObject &value) noexcept
    -> Result<FrameDisposition> {
    auto result = detail::protect([&] {
        require(!failed, ErrorCode::wrongPhase);
        const auto index = flow(route);
        const auto encoded = get(encodeFrameObject(value, contextValue));
        if (!pending[index]) {
            require(index == 0, ErrorCode::wrongPhase);
            recoveryNeeded = true;
            return FrameDisposition::needsRecovery;
        }
        auto &stage = *pending[index];
        if (stage.discard || (current() && stage.begin.id.value <= current()->begin.id.value)) {
            return FrameDisposition::obsolete;
        }
        if (index == 0 && value.frame.value > stage.begin.id.value) {
            pending[index].reset();
            recoveryNeeded = true;
            return FrameDisposition::needsRecovery;
        }
        require(value.frame == stage.begin.id);
        objectRoute(route, stage.route, value.key.kind);
        require(stage.changes.empty() || stage.changes.back().key < value.key);
        require(stage.changes.size() < stage.begin.changeCount);
        within(add(stage.bytes, encoded.size()), stage.begin.changeBytes.value);
        stage.bytes += encoded.size();
        auto owned = multiply(value.fields.size(), sizeof(std::optional<FieldValue>));
        for (const auto &field : value.fields) {
            if (field) {
                owned = add(owned, ownedFieldBytes(*field));
            }
        }
        within(add(stage.ownedBytes, owned), contextValue.records.ownedBytes.value);
        stage.ownedBytes += owned;
        stage.changes.push_back(value);
        return FrameDisposition::staged;
    });
    if (!result) {
        return failure(result.error());
    }
    return result;
}
auto FrameReceiver::end(Route route, FrameEnd frame) noexcept -> Result<FrameDisposition> {
    auto result = detail::protect([&] {
        require(!failed, ErrorCode::wrongPhase);
        const auto index = flow(route);
        get(encodeFrameEnd(frame));
        if (!pending[index]) {
            require(index == 0, ErrorCode::wrongPhase);
            recoveryNeeded = true;
            return FrameDisposition::needsRecovery;
        }
        auto &stage = *pending[index];
        if (index == 0 && frame.id.value > stage.begin.id.value) {
            pending[index].reset();
            recoveryNeeded = true;
            return FrameDisposition::needsRecovery;
        }
        frameRoute(route, FrameKind::end, stage.begin);
        require(route.opcode == stage.route.opcode && frame.id == stage.begin.id &&
                frame.changeCount == stage.begin.changeCount);
        if (stage.discard || (current() && frame.id.value <= current()->begin.id.value)) {
            pending[index].reset();
            return FrameDisposition::obsolete;
        }
        if (stage.missingBase || stage.changes.size() != frame.changeCount ||
            stage.bytes != stage.begin.changeBytes.value) {
            require(index == 0, ErrorCode::malformed);
            pending[index].reset();
            recoveryNeeded = true;
            return FrameDisposition::needsRecovery;
        }
        const StateSnapshot *base{};
        if (stage.begin.base.value != 0) {
            const auto found = std::ranges::find_if(retained, [&](const auto &snapshot) {
                return snapshot->begin.id == stage.begin.base;
            });
            if (found == retained.end()) {
                require(index == 0, ErrorCode::sequence);
                pending[index].reset();
                recoveryNeeded = true;
                return FrameDisposition::needsRecovery;
            }
            base = found->get();
        }
        auto snapshot =
            get(reconstructState(stage.begin, stage.changes, base, contextValue, digestProvider));
        require(snapshot.digest == frame.snapshotHash, ErrorCode::digestMismatch);
        const auto recoveryComplete = snapshot.begin.base.value == 0 && index == 1;
        retained.push_back(std::make_shared<const StateSnapshot>(std::move(snapshot)));
        while (retained.size() > windowFrames) {
            retained.pop_front();
        }
        pending[index].reset();
        if (recoveryComplete) {
            recoveryNeeded = false;
            requestQueued = false;
        }
        return FrameDisposition::applied;
    });
    if (!result) {
        return failure(result.error());
    }
    return result;
}
auto FrameLedger::recordSent(std::shared_ptr<const StateSnapshot> snapshot, Delivery delivery,
                             bool recovery) noexcept -> Result<void> {
    return detail::protect([&] {
        require(windowFrames > 0 && snapshot && !snapshot->digest.isZero() && !recoveryFrame,
                ErrorCode::wrongPhase);
        validateFrameBegin(snapshot->begin);
        require(snapshot->begin.id.value > lastSent.value, ErrorCode::sequence);
        require(delivery == Delivery::reliable || delivery == Delivery::unreliable);
        if (!retained.empty()) {
            const auto &prior = *retained.back().snapshot;
            require(snapshot->epoch == prior.epoch && snapshot->catalogHash == prior.catalogHash &&
                        snapshot->schemaHash == prior.schemaHash,
                    ErrorCode::staleContext);
        }
        require(!recovery || (delivery == Delivery::reliable && snapshot->begin.base.value == 0));
        if (snapshot->begin.base.value != 0) {
            const auto found = std::ranges::find_if(retained, [&](const Sent &sent) {
                return sent.snapshot->begin.id == snapshot->begin.base && sent.acknowledged;
            });
            require(found != retained.end(), ErrorCode::sequence);
        }
        retained.push_back({snapshot, false});
        lastSent = snapshot->begin.id;
        if (recovery) {
            recoveryFrame = FrameAck{snapshot->begin.id, snapshot->digest};
        }
        while (retained.size() > windowFrames) {
            retiredThrough = retained.front().snapshot->begin.id;
            retained.pop_front();
        }
    });
}
auto FrameLedger::acknowledge(FrameAck ack) noexcept -> Result<bool> {
    return detail::protect([&] {
        get(encodeFrameAck(ack));
        require(ack.id.value <= lastSent.value, ErrorCode::sequence);
        if (ack.id.value <= retiredThrough.value) {
            return false;
        }
        const auto found = std::ranges::find_if(
            retained, [&](const Sent &sent) { return sent.snapshot->begin.id == ack.id; });
        require(found != retained.end(), ErrorCode::sequence);
        require(found->snapshot->digest == ack.snapshotHash, ErrorCode::digestMismatch);
        found->acknowledged = true;
        if (recoveryFrame == ack) {
            recoveryFrame.reset();
        }
        return true;
    });
}
auto FrameLedger::base() const noexcept -> std::shared_ptr<const StateSnapshot> {
    for (auto found = retained.rbegin(); found != retained.rend(); ++found) {
        if (found->acknowledged) {
            return found->snapshot;
        }
    }
    return nullptr;
}
} // namespace extendedPrecache::data
