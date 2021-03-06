/*
 * Copyright 2001-2008, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Adi Oanca <adioanca@mymail.ro>
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Axel Dörfler, axeld@pinc-software.de
 *		Michael Pfeiffer <laplace@users.sourceforge.net>
 */

//!	Data classes for working with BView states and draw parameters

#include "DrawState.h"

#include <new>
#include <stdio.h>

#include <Region.h>

#include "AlphaMask.h"
#include "LinkReceiver.h"
#include "LinkSender.h"
#include "ServerProtocolStructs.h"


using std::nothrow;


DrawState::DrawState()
	:
	fOrigin(0.0f, 0.0f),
	fCombinedOrigin(0.0f, 0.0f),
	fScale(1.0f),
	fCombinedScale(1.0f),
	fTransform(),
	fCombinedTransform(),
	fClippingRegion(NULL),
	fAlphaMask(NULL),

	fHighColor((rgb_color){ 0, 0, 0, 255 }),
	fLowColor((rgb_color){ 255, 255, 255, 255 }),
	fPattern(kSolidHigh),

	fDrawingMode(B_OP_COPY),
	fAlphaSrcMode(B_PIXEL_ALPHA),
	fAlphaFncMode(B_ALPHA_OVERLAY),

	fPenLocation(0.0f, 0.0f),
	fPenSize(1.0f),

	fFontAliasing(false),
	fSubPixelPrecise(false),
	fLineCapMode(B_BUTT_CAP),
	fLineJoinMode(B_MITER_JOIN),
	fMiterLimit(B_DEFAULT_MITER_LIMIT),
	fFillRule(B_NONZERO),
	fPreviousState(NULL)
{
	fUnscaledFontSize = fFont.Size();
}


DrawState::DrawState(const DrawState& other)
	:
	fOrigin(other.fOrigin),
	fCombinedOrigin(other.fCombinedOrigin),
	fScale(other.fScale),
	fCombinedScale(other.fCombinedScale),
	fTransform(other.fTransform),
	fCombinedTransform(other.fCombinedTransform),
	fClippingRegion(NULL),
	fAlphaMask(NULL),

	fHighColor(other.fHighColor),
	fLowColor(other.fLowColor),
	fPattern(other.fPattern),

	fDrawingMode(other.fDrawingMode),
	fAlphaSrcMode(other.fAlphaSrcMode),
	fAlphaFncMode(other.fAlphaFncMode),

	fPenLocation(other.fPenLocation),
	fPenSize(other.fPenSize),

	fFont(other.fFont),
	fFontAliasing(other.fFontAliasing),

	fSubPixelPrecise(other.fSubPixelPrecise),

	fLineCapMode(other.fLineCapMode),
	fLineJoinMode(other.fLineJoinMode),
	fMiterLimit(other.fMiterLimit),
	fFillRule(other.fFillRule),

	// Since fScale is reset to 1.0, the unscaled
	// font size is the current size of the font
	// (which is from->fUnscaledFontSize * from->fCombinedScale)
	fUnscaledFontSize(other.fUnscaledFontSize),
	fPreviousState(NULL)
{
}


DrawState::~DrawState()
{
	delete fClippingRegion;
	delete fPreviousState;
	if (fAlphaMask != NULL)
		fAlphaMask->ReleaseReference();
}


DrawState*
DrawState::PushState()
{
	DrawState* next = new (nothrow) DrawState(*this);

	if (next != NULL) {
		// Prepare state as derived from this state
		next->fOrigin = BPoint(0.0, 0.0);
		next->fScale = 1.0;
		next->fTransform.Reset();
		next->fPreviousState = this;
		next->SetAlphaMask(fAlphaMask);
	}

	return next;
}


DrawState*
DrawState::PopState()
{
	DrawState* previous = PreviousState();

	fPreviousState = NULL;
	delete this;

	return previous;
}


void
DrawState::ReadFontFromLink(BPrivate::LinkReceiver& link)
{
	uint16 mask;
	link.Read<uint16>(&mask);

	if ((mask & B_FONT_FAMILY_AND_STYLE) != 0) {
		uint32 fontID;
		link.Read<uint32>(&fontID);
		fFont.SetFamilyAndStyle(fontID);
	}

	if ((mask & B_FONT_SIZE) != 0) {
		float size;
		link.Read<float>(&size);
		fUnscaledFontSize = size;
		fFont.SetSize(fUnscaledFontSize * fCombinedScale);
	}

	if ((mask & B_FONT_SHEAR) != 0) {
		float shear;
		link.Read<float>(&shear);
		fFont.SetShear(shear);
	}

	if ((mask & B_FONT_ROTATION) != 0) {
		float rotation;
		link.Read<float>(&rotation);
		fFont.SetRotation(rotation);
	}

	if ((mask & B_FONT_FALSE_BOLD_WIDTH) != 0) {
		float falseBoldWidth;
		link.Read<float>(&falseBoldWidth);
		fFont.SetFalseBoldWidth(falseBoldWidth);
	}

	if ((mask & B_FONT_SPACING) != 0) {
		uint8 spacing;
		link.Read<uint8>(&spacing);
		fFont.SetSpacing(spacing);
	}

	if ((mask & B_FONT_ENCODING) != 0) {
		uint8 encoding;
		link.Read<uint8>(&encoding);
		fFont.SetEncoding(encoding);
	}

	if ((mask & B_FONT_FACE) != 0) {
		uint16 face;
		link.Read<uint16>(&face);
		fFont.SetFace(face);
	}

	if ((mask & B_FONT_FLAGS) != 0) {
		uint32 flags;
		link.Read<uint32>(&flags);
		fFont.SetFlags(flags);
	}
}


void
DrawState::ReadFromLink(BPrivate::LinkReceiver& link)
{
	ViewSetStateInfo info;

	link.Read<ViewSetStateInfo>(&info);
	
	fPenLocation = info.penLocation;
	fPenSize = info.penSize;
	fHighColor = info.highColor;
	fLowColor = info.lowColor;
	fPattern = info.pattern;
	fDrawingMode = info.drawingMode;
	fOrigin = info.origin;
	fScale = info.scale;
	fTransform = info.transform;
	fLineJoinMode = info.lineJoin;
	fLineCapMode = info.lineCap;
	fMiterLimit = info.miterLimit;
	fFillRule = info.fillRule;
	fAlphaSrcMode = info.alphaSourceMode;
	fAlphaFncMode = info.alphaFunctionMode;
	fFontAliasing = info.fontAntialiasing;

	if (fPreviousState != NULL) {
		fCombinedOrigin = fPreviousState->fCombinedOrigin + fOrigin;
		fCombinedScale = fPreviousState->fCombinedScale * fScale;
		fCombinedTransform = fPreviousState->fCombinedTransform * fTransform;
	} else {
		fCombinedOrigin = fOrigin;
		fCombinedScale = fScale;
		fCombinedTransform = fTransform;
	}


	// read clipping
	// TODO: This could be optimized, but the user clipping regions are rarely
	// used, so it's low priority...
	int32 clipRectCount;
	link.Read<int32>(&clipRectCount);

	if (clipRectCount >= 0) {
		BRegion region;
		BRect rect;
		for (int32 i = 0; i < clipRectCount; i++) {
			link.Read<BRect>(&rect);
			region.Include(rect);
		}
		SetClippingRegion(&region);
	} else {
		// No user clipping used
		SetClippingRegion(NULL);
	}
}


void
DrawState::WriteToLink(BPrivate::LinkSender& link) const
{
	// Attach font state
	ViewGetStateInfo info;
	info.fontID = fFont.GetFamilyAndStyle();
	info.fontSize = fFont.Size();
	info.fontShear = fFont.Shear();
	info.fontRotation = fFont.Rotation();
	info.fontFalseBoldWidth = fFont.FalseBoldWidth();
	info.fontSpacing = fFont.Spacing();
	info.fontEncoding = fFont.Encoding();
	info.fontFace = fFont.Face();
	info.fontFlags = fFont.Flags();

	// Attach view state
	info.viewStateInfo.penLocation = fPenLocation;
	info.viewStateInfo.penSize = fPenSize;
	info.viewStateInfo.highColor = fHighColor;
	info.viewStateInfo.lowColor = fLowColor;
	info.viewStateInfo.pattern = (::pattern)fPattern.GetPattern();
	info.viewStateInfo.drawingMode = fDrawingMode;
	info.viewStateInfo.origin = fOrigin;
	info.viewStateInfo.scale = fScale;
	info.viewStateInfo.transform = fTransform;
	info.viewStateInfo.lineJoin = fLineJoinMode;
	info.viewStateInfo.lineCap = fLineCapMode;
	info.viewStateInfo.miterLimit = fMiterLimit;
	info.viewStateInfo.fillRule = fFillRule;
	info.viewStateInfo.alphaSourceMode = fAlphaSrcMode;
	info.viewStateInfo.alphaFunctionMode = fAlphaFncMode;
	info.viewStateInfo.fontAntialiasing = fFontAliasing;


	link.Attach<ViewGetStateInfo>(info);


	// TODO: Could be optimized, but is low prio, since most views do not
	// use a custom clipping region...
	if (fClippingRegion != NULL) {
		int32 clippingRectCount = fClippingRegion->CountRects();
		link.Attach<int32>(clippingRectCount);
		for (int i = 0; i < clippingRectCount; i++)
			link.Attach<BRect>(fClippingRegion->RectAt(i));
	} else {
		// no client clipping
		link.Attach<int32>(-1);
	}
}


void
DrawState::SetOrigin(BPoint origin)
{
	fOrigin = origin;

	// NOTE: the origins of earlier states are never expected to
	// change, only the topmost state ever changes
	if (fPreviousState != NULL) {
		fCombinedOrigin.x = fPreviousState->fCombinedOrigin.x
			+ fOrigin.x * fPreviousState->fCombinedScale;
		fCombinedOrigin.y = fPreviousState->fCombinedOrigin.y
			+ fOrigin.y * fPreviousState->fCombinedScale;
	} else {
		fCombinedOrigin = fOrigin;
	}
}


void
DrawState::SetScale(float scale)
{
	if (fScale == scale)
		return;

	fScale = scale;

	// NOTE: the scales of earlier states are never expected to
	// change, only the topmost state ever changes
	if (fPreviousState != NULL)
		fCombinedScale = fPreviousState->fCombinedScale * fScale;
	else
		fCombinedScale = fScale;

	// update font size
	// NOTE: This is what makes the call potentially expensive,
	// hence the introductory check
	fFont.SetSize(fUnscaledFontSize * fCombinedScale);
}


void
DrawState::SetTransform(BAffineTransform transform)
{
	if (fTransform == transform)
		return;

	fTransform = transform;

	// NOTE: the transforms of earlier states are never expected to
	// change, only the topmost state ever changes
	if (fPreviousState != NULL)
		fCombinedTransform = fPreviousState->fCombinedTransform * fTransform;
	else
		fCombinedTransform = fTransform;
}


void
DrawState::SetClippingRegion(const BRegion* region)
{
	if (region) {
		if (fClippingRegion != NULL)
			*fClippingRegion = *region;
		else
			fClippingRegion = new(nothrow) BRegion(*region);
	} else {
		delete fClippingRegion;
		fClippingRegion = NULL;
	}
}


bool
DrawState::HasClipping() const
{
	if (fClippingRegion != NULL)
		return true;
	if (fPreviousState != NULL)
		return fPreviousState->HasClipping();
	return false;
}


bool
DrawState::HasAdditionalClipping() const
{
	return fClippingRegion != NULL;
}


bool
DrawState::GetCombinedClippingRegion(BRegion* region) const
{
	if (fClippingRegion != NULL) {
		BRegion localTransformedClipping(*fClippingRegion);
		Transform(&localTransformedClipping);

		if (fPreviousState != NULL
			&& fPreviousState->GetCombinedClippingRegion(region)) {
			localTransformedClipping.IntersectWith(region);
		}
		*region = localTransformedClipping;
		return true;
	} else {
		if (fPreviousState != NULL)
			return fPreviousState->GetCombinedClippingRegion(region);
	}
	return false;
}


void
DrawState::SetAlphaMask(AlphaMask* mask)
{
	// NOTE: In BeOS, it wasn't possible to clip to a BPicture and keep
	// regular custom clipping to a BRegion at the same time.
	if (fAlphaMask == mask)
		return;

	if (mask != NULL)
		mask->AcquireReference();
	if (fAlphaMask != NULL)
		fAlphaMask->ReleaseReference();
	fAlphaMask = mask;
	if (fAlphaMask != NULL && fPreviousState != NULL)
		fAlphaMask->SetPrevious(fPreviousState->fAlphaMask);
		
}


AlphaMask*
DrawState::GetAlphaMask() const
{
	return fAlphaMask;
}


// #pragma mark -


void
DrawState::Transform(float* x, float* y) const
{
	// scale relative to origin, therefore
	// scale first then translate to
	// origin
	*x *= fCombinedScale;
	*y *= fCombinedScale;
	*x += fCombinedOrigin.x;
	*y += fCombinedOrigin.y;
}


void
DrawState::InverseTransform(float* x, float* y) const
{
	*x -= fCombinedOrigin.x;
	*y -= fCombinedOrigin.y;
	if (fCombinedScale != 0.0) {
		*x /= fCombinedScale;
		*y /= fCombinedScale;
	}
}


void
DrawState::Transform(BPoint* point) const
{
	Transform(&(point->x), &(point->y));
}


void
DrawState::Transform(BRect* rect) const
{
	Transform(&(rect->left), &(rect->top));
	Transform(&(rect->right), &(rect->bottom));
}


void
DrawState::Transform(BRegion* region) const
{
	if (fCombinedScale == 1.0) {
		region->OffsetBy(fCombinedOrigin.x, fCombinedOrigin.y);
	} else {
		// TODO: optimize some more
		BRegion converted;
		int32 count = region->CountRects();
		for (int32 i = 0; i < count; i++) {
			BRect r = region->RectAt(i);
			BPoint lt(r.LeftTop());
			BPoint rb(r.RightBottom());
			// offset to bottom right corner of pixel before transformation
			rb.x++;
			rb.y++;
			// apply transformation
			Transform(&lt.x, &lt.y);
			Transform(&rb.x, &rb.y);
			// reset bottom right to pixel "index"
			rb.x--;
			rb.y--;
			// add rect to converted region
			// NOTE/TODO: the rect would not have to go
			// through the whole intersection test process,
			// it is guaranteed not to overlap with any rect
			// already contained in the region
			converted.Include(BRect(lt, rb));
		}
		*region = converted;
	}
}


void
DrawState::InverseTransform(BPoint* point) const
{
	InverseTransform(&(point->x), &(point->y));
}


// #pragma mark -


void
DrawState::SetHighColor(rgb_color color)
{
	fHighColor = color;
}


void
DrawState::SetLowColor(rgb_color color)
{
	fLowColor = color;
}


void
DrawState::SetPattern(const Pattern& pattern)
{
	fPattern = pattern;
}


void
DrawState::SetDrawingMode(drawing_mode mode)
{
	fDrawingMode = mode;
}


void
DrawState::SetBlendingMode(source_alpha srcMode, alpha_function fncMode)
{
	fAlphaSrcMode = srcMode;
	fAlphaFncMode = fncMode;
}


void
DrawState::SetPenLocation(BPoint location)
{
	fPenLocation = location;
}


BPoint
DrawState::PenLocation() const
{
	return fPenLocation;
}


void
DrawState::SetPenSize(float size)
{
	fPenSize = size;
}


//! returns the scaled pen size
float
DrawState::PenSize() const
{
	float penSize = fPenSize * fCombinedScale;
	// NOTE: As documented in the BeBook,
	// pen size is never smaller than 1.0.
	// This is supposed to be the smallest
	// possible device size.
	if (penSize < 1.0)
		penSize = 1.0;
	return penSize;
}


//! returns the unscaled pen size
float
DrawState::UnscaledPenSize() const
{
	// NOTE: As documented in the BeBook,
	// pen size is never smaller than 1.0.
	// This is supposed to be the smallest
	// possible device size.
	return max_c(fPenSize, 1.0);
}


//! sets the font to be already scaled by fScale
void
DrawState::SetFont(const ServerFont& font, uint32 flags)
{
	if (flags == B_FONT_ALL) {
		fFont = font;
		fUnscaledFontSize = font.Size();
		fFont.SetSize(fUnscaledFontSize * fCombinedScale);
	} else {
		// family & style
		if ((flags & B_FONT_FAMILY_AND_STYLE) != 0)
			fFont.SetFamilyAndStyle(font.GetFamilyAndStyle());
		// size
		if ((flags & B_FONT_SIZE) != 0) {
			fUnscaledFontSize = font.Size();
			fFont.SetSize(fUnscaledFontSize * fCombinedScale);
		}
		// shear
		if ((flags & B_FONT_SHEAR) != 0)
			fFont.SetShear(font.Shear());
		// rotation
		if ((flags & B_FONT_ROTATION) != 0)
			fFont.SetRotation(font.Rotation());
		// spacing
		if ((flags & B_FONT_SPACING) != 0)
			fFont.SetSpacing(font.Spacing());
		// encoding
		if ((flags & B_FONT_ENCODING) != 0)
			fFont.SetEncoding(font.Encoding());
		// face
		if ((flags & B_FONT_FACE) != 0)
			fFont.SetFace(font.Face());
		// flags
		if ((flags & B_FONT_FLAGS) != 0)
			fFont.SetFlags(font.Flags());
	}
}


void
DrawState::SetForceFontAliasing(bool aliasing)
{
	fFontAliasing = aliasing;
}


void
DrawState::SetSubPixelPrecise(bool precise)
{
	fSubPixelPrecise = precise;
}


void
DrawState::SetLineCapMode(cap_mode mode)
{
	fLineCapMode = mode;
}


void
DrawState::SetLineJoinMode(join_mode mode)
{
	fLineJoinMode = mode;
}


void
DrawState::SetMiterLimit(float limit)
{
	fMiterLimit = limit;
}


void
DrawState::SetFillRule(int32 fillRule)
{
	fFillRule = fillRule;
}


void
DrawState::PrintToStream() const
{
	printf("\t Origin: (%.1f, %.1f)\n", fOrigin.x, fOrigin.y);
	printf("\t Scale: %.2f\n", fScale);
	printf("\t Transform: %.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n",
		fTransform.sx, fTransform.shy, fTransform.shx,
		fTransform.sy, fTransform.tx, fTransform.ty);

	printf("\t Pen Location and Size: (%.1f, %.1f) - %.2f (%.2f)\n",
		   fPenLocation.x, fPenLocation.y, PenSize(), fPenSize);

	printf("\t HighColor: r=%d g=%d b=%d a=%d\n",
		fHighColor.red, fHighColor.green, fHighColor.blue, fHighColor.alpha);
	printf("\t LowColor: r=%d g=%d b=%d a=%d\n",
		fLowColor.red, fLowColor.green, fLowColor.blue, fLowColor.alpha);
	printf("\t Pattern: %" B_PRIu64 "\n", fPattern.GetInt64());

	printf("\t DrawMode: %" B_PRIu32 "\n", (uint32)fDrawingMode);
	printf("\t AlphaSrcMode: %" B_PRId32 "\t AlphaFncMode: %" B_PRId32 "\n",
		   (int32)fAlphaSrcMode, (int32)fAlphaFncMode);

	printf("\t LineCap: %d\t LineJoin: %d\t MiterLimit: %.2f\n",
		   (int16)fLineCapMode, (int16)fLineJoinMode, fMiterLimit);

	if (fClippingRegion != NULL)
		fClippingRegion->PrintToStream();

	printf("\t ===== Font Data =====\n");
	printf("\t Style: CURRENTLY NOT SET\n"); // ???
	printf("\t Size: %.1f (%.1f)\n", fFont.Size(), fUnscaledFontSize);
	printf("\t Shear: %.2f\n", fFont.Shear());
	printf("\t Rotation: %.2f\n", fFont.Rotation());
	printf("\t Spacing: %" B_PRId32 "\n", fFont.Spacing());
	printf("\t Encoding: %" B_PRId32 "\n", fFont.Encoding());
	printf("\t Face: %d\n", fFont.Face());
	printf("\t Flags: %" B_PRIu32 "\n", fFont.Flags());
}

