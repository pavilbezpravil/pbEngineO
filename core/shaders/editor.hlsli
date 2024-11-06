#include "shared/hlslCppShared.hlsli"
#include "math.hlsli"

cbuffer gEditorCB : DECLARE_REGISTER(b, CB_SLOT_EDITOR, REGISTER_SPACE_COMMON) {
  SEditorCB gEditor;
}

RWStructuredBuffer<uint> gUnderCursorBuffer : DECLARE_REGISTER(u, UAV_SLOT_UNDER_CURSOR_BUFFER, REGISTER_SPACE_COMMON);

void SetEntityUnderCursor(int2 pixelIdx, uint entityID) {
    if (all(pixelIdx == gEditor.cursorPixelIdx) && entityID != UINT_MAX) {
        gUnderCursorBuffer[0] = entityID; // todo: atomic add
    }
}
