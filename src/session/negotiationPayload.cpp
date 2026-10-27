#include <extendedPrecache/session/negotiation.hpp>

#include "negotiationOrder.hpp"

namespace extendedPrecache::session {
auto NegotiationSession::makePayload() const -> std::expected<FramePayload, Rejection> {
    const auto &next = detail::order[step];
    switch (next.kind) {
    case MessageKind::serverOffer:
        if (!offer) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return *offer;
    case MessageKind::applicationContract:
        return DigestBody<MessageKind::applicationContract>{contract.application};
    case MessageKind::dataContract:
        return DigestBody<MessageKind::dataContract>{contract.data};
    case MessageKind::catalogIdentity:
        if (!snapshot) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return DigestBody<MessageKind::catalogIdentity>{snapshot.value().catalog};
    case MessageKind::requirementsPage:
        if (!snapshot) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return detail::page<MessageKind::requirementsPage>(snapshot.value().requirements.vector(),
                                                           next.page);
    case MessageKind::serverLimitsPage:
        if (!serverLimits) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return detail::page<MessageKind::serverLimitsPage>(serverLimits.value().vector(),
                                                           next.page);
    case MessageKind::clientLimitsPage:
        if (!clientLimits) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return detail::page<MessageKind::clientLimitsPage>(clientLimits.value().vector(),
                                                           next.page);
    case MessageKind::selectionPage:
        if (!selected) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return detail::page<MessageKind::selectionPage>(selected.value().vector(), next.page);
    case MessageKind::clientHello:
        return ClientHelloBody{config.values().policy, clientNonce,
                               config.values().negotiation.timeout};
    case MessageKind::offerReference:
        return DigestBody<MessageKind::offerReference>{offerHash};
    case MessageKind::offerEnd: {
        const auto digest = hash(next.kind);
        if (!digest) {
            return std::unexpected{digest.error()};
        }
        return DigestBody<MessageKind::offerEnd>{*digest};
    }
    case MessageKind::clientCapabilitiesEnd: {
        const auto digest = hash(next.kind);
        if (!digest) {
            return std::unexpected{digest.error()};
        }
        return DigestBody<MessageKind::clientCapabilitiesEnd>{*digest};
    }
    case MessageKind::selectionEnd: {
        const auto digest = hash(next.kind);
        if (!digest) {
            return std::unexpected{digest.error()};
        }
        return DigestBody<MessageKind::selectionEnd>{*digest};
    }
    case MessageKind::clientReady:
        return DigestBody<MessageKind::clientReady>{selectionHash};
    case MessageKind::serverCommit:
        return DigestBody<MessageKind::serverCommit>{selectionHash};
    case MessageKind::clientCommitAck:
        return DigestBody<MessageKind::clientCommitAck>{selectionHash};
    case MessageKind::reject:
        break;
    }
    return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
}

auto NegotiationSession::validatePayload(const Frame &frame) -> std::expected<void, Rejection> {
    const auto &payload = frame.payload;
    const auto kind = messageKind(payload);
    switch (kind) {
    case MessageKind::serverOffer: {
        const auto &body = std::get<ServerOfferBody>(payload);
        if (body.dataContract != contract.id) {
            return std::unexpected{Rejection{RejectCode::unsupportedContract}};
        }
        offer = body;
        timeout.value = std::min(timeout.value, body.timeout.value);
        return {};
    }
    case MessageKind::clientHello: {
        const auto &body = std::get<ClientHelloBody>(payload);
        clientNonce = body.nonce;
        timeout.value = std::min(timeout.value, body.timeout.value);
        return {};
    }
    case MessageKind::applicationContract:
        if (std::get<DigestBody<MessageKind::applicationContract>>(payload).value !=
            contract.application) {
            return std::unexpected{Rejection{RejectCode::unsupportedContract}};
        }
        return {};
    case MessageKind::dataContract:
        if (std::get<DigestBody<MessageKind::dataContract>>(payload).value != contract.data) {
            return std::unexpected{Rejection{RejectCode::unsupportedContract}};
        }
        return {};
    case MessageKind::catalogIdentity:
        catalog = std::get<DigestBody<MessageKind::catalogIdentity>>(payload).value;
        if (catalog.isZero()) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        return {};
    case MessageKind::requirementsPage: {
        const auto result = detail::gather<MessageKind::requirementsPage>(
            payload, detail::order[step].page, requirementValues);
        if (!result) {
            return result;
        }
        if (detail::order[step].page.value + 1 == pageCount) {
            if (!offer) {
                return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
            }
            const auto requirements = MapRequirements::create(CapabilityVector{requirementValues});
            if (!requirements) {
                return std::unexpected{requirements.error()};
            }
            snapshot =
                MapSnapshot{offer.value().spawn, offer.value().epoch, catalog, *requirements};
        }
        return {};
    }
    case MessageKind::serverLimitsPage:
    case MessageKind::clientLimitsPage: {
        const auto ordinal = detail::order[step].page;
        const auto result =
            kind == MessageKind::serverLimitsPage
                ? detail::gather<MessageKind::serverLimitsPage>(payload, ordinal, limitValues)
                : detail::gather<MessageKind::clientLimitsPage>(payload, ordinal, limitValues);
        if (!result) {
            return result;
        }
        if (ordinal.value + 1 == pageCount) {
            const auto limits = CapabilityLimits::create(CapabilityVector{limitValues});
            if (!limits) {
                return std::unexpected{limits.error()};
            }
            if (const auto valid =
                    validateChunkBudget(*limits, contract.requirements.chunkOverhead);
                !valid) {
                return valid;
            }
            if (limits->vector().get<CapabilityField::consistencyEntries>().value >
                limits->vector().get<CapabilityField::resourceEntries>().value) {
                return std::unexpected{
                    Rejection{RejectCode::invalidConfiguration,
                              static_cast<std::uint8_t>(CapabilityField::consistencyEntries)}};
            }
            if (kind == MessageKind::serverLimitsPage) {
                serverLimits = *limits;
            } else {
                clientLimits = *limits;
            }
            if (serverLimits && clientLimits) {
                if (!snapshot) {
                    return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
                }
                selected = intersectLimits(*serverLimits, *clientLimits);
                if (const auto valid =
                        validateChunkBudget(*selected, contract.requirements.chunkOverhead);
                    !valid) {
                    return valid;
                }
                return admitRequirements(snapshot.value().requirements, *selected);
            }
        }
        return {};
    }
    case MessageKind::selectionPage: {
        if (!selected) {
            return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
        }
        const auto result = detail::gather<MessageKind::selectionPage>(
            payload, detail::order[step].page, selectionValues);
        if (!result) {
            return result;
        }
        if (detail::order[step].page.value + 1 == pageCount &&
            CapabilityVector{selectionValues} != selected.value().vector()) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
        }
        return {};
    }
    case MessageKind::offerEnd:
    case MessageKind::clientCapabilitiesEnd:
    case MessageKind::selectionEnd: {
        const auto computed = hash(kind);
        if (!computed) {
            return std::unexpected{computed.error()};
        }
        const auto declared = std::visit(
            [](const auto &body) -> Digest {
                if constexpr (requires { body.value; } &&
                              !std::same_as<std::remove_cvref_t<decltype(body)>, RejectBody>) {
                    return body.value;
                } else {
                    return {};
                }
            },
            payload);
        if (declared != *computed) {
            return std::unexpected{Rejection{RejectCode::transcriptMismatch}};
        }
        if (kind == MessageKind::offerEnd) {
            offerHash = *computed;
        } else if (kind == MessageKind::clientCapabilitiesEnd) {
            clientHash = *computed;
        } else {
            selectionHash = *computed;
        }
        return {};
    }
    case MessageKind::offerReference:
        if (std::get<DigestBody<MessageKind::offerReference>>(payload).value != offerHash) {
            return std::unexpected{Rejection{RejectCode::transcriptMismatch}};
        }
        return {};
    case MessageKind::clientReady:
        if (std::get<DigestBody<MessageKind::clientReady>>(payload).value != selectionHash) {
            return std::unexpected{Rejection{RejectCode::transcriptMismatch}};
        }
        return {};
    case MessageKind::serverCommit:
        if (std::get<DigestBody<MessageKind::serverCommit>>(payload).value != selectionHash) {
            return std::unexpected{Rejection{RejectCode::transcriptMismatch}};
        }
        return checkStorage();
    case MessageKind::clientCommitAck:
        if (std::get<DigestBody<MessageKind::clientCommitAck>>(payload).value != selectionHash) {
            return std::unexpected{Rejection{RejectCode::transcriptMismatch}};
        }
        return checkStorage();
    case MessageKind::reject:
        break;
    }
    return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
}
} // namespace extendedPrecache::session
