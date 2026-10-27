// Normative layouts from docs/data/effectSchemasV2.json; no native bindings.

constexpr auto effect0Fields = std::array{
    EffectField{"start", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"end", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect1Fields = std::array{
    EffectField{"startEntity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"end", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect3Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"frameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"flags", ValueKind::u8, std::nullopt, 0, false, 15},
};

constexpr auto effect5Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"frameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect7Fields = std::array{
    EffectField{"start", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"end", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"width", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"noise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
};

constexpr auto effect8Fields = std::array{
    EffectField{"startEntity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"endEntity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect13Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"decal", ValueKind::resource, ResourceKind::decal, 256, false, std::nullopt},
    EffectField{"entity", ValueKind::entity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"model", ValueKind::resource, ResourceKind::model, 24, true, std::nullopt},
};

constexpr auto effect15Fields = std::array{
    EffectField{"start", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"end", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"randomness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect17Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"brightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect18Fields = std::array{
    EffectField{"start", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"end", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"beamSprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"endSprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
};

constexpr auto effect19Fields = std::array{
    EffectField{"center", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"axis", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect20Fields = std::array{
    EffectField{"center", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"axis", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect21Fields = std::array{
    EffectField{"center", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"axis", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect22Fields = std::array{
    EffectField{"entity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"width", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"colorRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"colorGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"colorBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"brightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect23Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"brightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect24Fields = std::array{
    EffectField{"startEntity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"endEntity", ValueKind::packedEntity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"styleStartFrame", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleFrameRate", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleLife", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleWidth", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleNoise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleRed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleGreen", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBlue", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleBrightness", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"styleScroll", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect100Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"flags", ValueKind::i16, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect104Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"decal", ValueKind::resource, ResourceKind::decal, 256, false, std::nullopt},
    EffectField{"entity", ValueKind::entity16, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect105Fields = std::array{
    EffectField{"entity", ValueKind::entity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"density", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect106Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"yaw", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"model", ValueKind::resource, ResourceKind::model, 2, false, std::nullopt},
    EffectField{"bounce", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect107Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"model", ValueKind::resource, ResourceKind::model, 2, false, std::nullopt},
    EffectField{"count", ValueKind::u16, std::nullopt, 0, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect108Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"size", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"randomVelocity", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"modelSprite", ValueKind::resource, ResourceKind::model, 6, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"flags", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect109Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"entity", ValueKind::entity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"decal", ValueKind::resource, ResourceKind::decal, 256, false, std::nullopt},
};

constexpr auto effect110Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"speed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"noise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect112Fields = std::array{
    EffectField{"player", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"entity", ValueKind::entity16, std::nullopt, 0, false, std::nullopt},
    EffectField{"spray", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect113Fields = std::array{
    EffectField{"mins", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"maxs", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"height", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"speed", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect114Fields = std::array{
    EffectField{"mins", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"maxs", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"height", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"speed", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect115Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"spraySprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"dropSprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"color", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"scale", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect116Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"decal", ValueKind::resource, ResourceKind::decal, 256, false, std::nullopt},
};

constexpr auto effect119Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"velocity", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"model", ValueKind::resource, ResourceKind::model, 2, false, std::nullopt},
    EffectField{"life", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"owner", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect120Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"direction", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"modelSprite", ValueKind::resource, ResourceKind::model, 6, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"speed", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"noise", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"renderMode", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect121Fields = std::array{
    EffectField{"player", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"sprite", ValueKind::resource, ResourceKind::model, 4, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"variance", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect123Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"radius", ValueKind::i16, std::nullopt, 0, false, std::nullopt},
    EffectField{"modelSprite", ValueKind::resource, ResourceKind::model, 6, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"flags", ValueKind::u8, std::nullopt, 0, false, 63},
    EffectField{"duration", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect124Fields = std::array{
    EffectField{"player", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"verticalOffset", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"model", ValueKind::resource, ResourceKind::model, 2, false, std::nullopt},
    EffectField{"life", ValueKind::u16, std::nullopt, 0, false, std::nullopt},
};

constexpr auto effect126Fields = std::array{
    EffectField{"origin", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"direction", ValueKind::coords3, std::nullopt, 0, false, std::nullopt},
    EffectField{"noiseX", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"noiseY", ValueKind::coord16, std::nullopt, 0, false, std::nullopt},
    EffectField{"count", ValueKind::u8, std::nullopt, 0, false, std::nullopt},
    EffectField{"decal", ValueKind::resource, ResourceKind::decal, 256, false, std::nullopt},
};

constexpr auto builtinEffects = std::array{
    BuiltinEffect{{0}, "beamPoints", effect0Fields, {26}},
    BuiltinEffect{{1}, "beamEntityPoint", effect1Fields, {22}},
    BuiltinEffect{{3}, "explosion", effect3Fields, {13}},
    BuiltinEffect{{5}, "smoke", effect5Fields, {12}},
    BuiltinEffect{{7}, "lightning", effect7Fields, {19}},
    BuiltinEffect{{8}, "beamEntities", effect8Fields, {18}},
    BuiltinEffect{{13}, "bspDecal", effect13Fields, {16}},
    BuiltinEffect{{15}, "spriteTrail", effect15Fields, {21}},
    BuiltinEffect{{17}, "sprite", effect17Fields, {12}},
    BuiltinEffect{{18}, "beamSprite", effect18Fields, {20}},
    BuiltinEffect{{19}, "beamTorus", effect19Fields, {26}},
    BuiltinEffect{{20}, "beamDisk", effect20Fields, {26}},
    BuiltinEffect{{21}, "beamCylinder", effect21Fields, {26}},
    BuiltinEffect{{22}, "beamFollow", effect22Fields, {12}},
    BuiltinEffect{{23}, "glowSprite", effect23Fields, {13}},
    BuiltinEffect{{24}, "beamRing", effect24Fields, {18}},
    BuiltinEffect{{100}, "largeFunnel", effect100Fields, {12}},
    BuiltinEffect{{104}, "decal", effect104Fields, {12}},
    BuiltinEffect{{105}, "fizz", effect105Fields, {7}},
    BuiltinEffect{{106}, "model", effect106Fields, {19}},
    BuiltinEffect{{107}, "explodeModel", effect107Fields, {15}},
    BuiltinEffect{{108}, "breakModel", effect108Fields, {26}},
    BuiltinEffect{{109}, "gunshotDecal", effect109Fields, {12}},
    BuiltinEffect{{110}, "spriteSpray", effect110Fields, {19}},
    BuiltinEffect{{112}, "playerDecal", effect112Fields, {10}},
    BuiltinEffect{{113}, "bubbles", effect113Fields, {21}},
    BuiltinEffect{{114}, "bubbleTrail", effect114Fields, {21}},
    BuiltinEffect{{115}, "bloodSprite", effect115Fields, {16}},
    BuiltinEffect{{116}, "worldDecal", effect116Fields, {10}},
    BuiltinEffect{{119}, "projectile", effect119Fields, {18}},
    BuiltinEffect{{120}, "spray", effect120Fields, {20}},
    BuiltinEffect{{121}, "playerSprites", effect121Fields, {7}},
    BuiltinEffect{{123}, "fireField", effect123Fields, {15}},
    BuiltinEffect{{124}, "playerAttachment", effect124Fields, {9}},
    BuiltinEffect{{126}, "multiGunshot", effect126Fields, {21}},
};
