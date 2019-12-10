//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2019 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#include "BilinearBlitter.hxx"

#include "ThreadDebugging.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BilinearBlitter::BilinearBlitter(FrameBufferSDL2& fb) :
  myTexture(nullptr),
  mySecondaryTexture(nullptr),
  myTexturesAreAllocated(false),
  myRecreateTextures(false),
  myStaticData(nullptr),
  myFB(fb)
{}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BilinearBlitter::~BilinearBlitter()
{
  free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BilinearBlitter::reinitialize(
  SDL_Rect srcRect,
  SDL_Rect destRect,
  FBSurface::Attributes attributes,
  SDL_Surface* staticData
)
{
  myRecreateTextures = !(
    mySrcRect.w == srcRect.w &&
    mySrcRect.h == srcRect.h &&
    myDstRect.w == destRect.w &&
    myDstRect.h == destRect.h &&
    attributes == myAttributes &&
    myStaticData == staticData
   );

   myStaticData = staticData;
   mySrcRect = srcRect;
   myDstRect = destRect;
   myAttributes = attributes;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BilinearBlitter::free()
{
  if (!myTexturesAreAllocated) {
    return;
  }

  ASSERT_MAIN_THREAD;

  SDL_Texture* textures[] = {myTexture, mySecondaryTexture};
  for (SDL_Texture* texture: textures) {
    if (!texture) continue;

    SDL_DestroyTexture(texture);
  }

  myTexturesAreAllocated = false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BilinearBlitter::blit(SDL_Surface& surface)
{
  ASSERT_MAIN_THREAD;

  recreateTexturesIfNecessary();

  SDL_Texture* texture = myTexture;

    if(myStaticData == nullptr) {
      SDL_UpdateTexture(myTexture, &mySrcRect, surface.pixels, surface.pitch);
      myTexture = mySecondaryTexture;
      mySecondaryTexture = texture;
    }

    SDL_RenderCopy(myFB.renderer(), texture, &mySrcRect, &myDstRect);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BilinearBlitter::recreateTexturesIfNecessary()
{
  if (myTexturesAreAllocated && !myRecreateTextures) {
    return;
  }

  ASSERT_MAIN_THREAD;

  if (myTexturesAreAllocated) {
    free();
  }

  SDL_TextureAccess texAccess = myStaticData == nullptr ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_STATIC;

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, myAttributes.smoothing ? "1" : "0");

  myTexture = SDL_CreateTexture(myFB.renderer(), myFB.pixelFormat().format,
      texAccess, mySrcRect.w, mySrcRect.h);

  if (myStaticData == nullptr) {
    mySecondaryTexture = SDL_CreateTexture(myFB.renderer(), myFB.pixelFormat().format,
        texAccess, mySrcRect.w, mySrcRect.h);
  } else {
    mySecondaryTexture = nullptr;
    SDL_UpdateTexture(myTexture, nullptr, myStaticData->pixels, myStaticData->pitch);
  }

  if (myAttributes.blending) {
    uInt8 blendAlpha = uInt8(myAttributes.blendalpha * 2.55);

    SDL_Texture* textures[] = {myTexture, mySecondaryTexture};
    for (SDL_Texture* texture: textures) {
      if (!texture) continue;

      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_SetTextureAlphaMod(texture, blendAlpha);
    }
  }

  myRecreateTextures = false;
  myTexturesAreAllocated = true;
}