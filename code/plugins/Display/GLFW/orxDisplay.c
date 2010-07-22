/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2010 Orx-Project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file orxDisplay.c
 * @date 23/06/2010
 * @author iarwain@orx-project.org
 *
 * GLFW display plugin implementation
 *
 */


#include "orxPluginAPI.h"

#ifdef __orxMAC__

  #define GL_GLEXT_PROTOTYPES

#endif /* __orxMAC__ */

#include "GL/glfw.h"
#include "GL/glext.h"
#include "SOIL.h"

#ifndef __orxEMBEDDED__
  #ifdef __orxMSVC__
    #pragma message("!!WARNING!! This plugin will only work in non-embedded mode when linked against a *DYNAMIC* version of GLFW!")
  #else /* __orxMSVC__ */
    #warning !!WARNING!! This plugin will only work in non-embedded mode when linked against a *DYNAMIC* version of SDL!
  #endif /* __orxMSVC__ */
#endif /* __orxEMBEDDED__ */


/** Module flags
 */
#define orxDISPLAY_KU32_STATIC_FLAG_NONE        0x00000000 /**< No flags */

#define orxDISPLAY_KU32_STATIC_FLAG_READY       0x00000001 /**< Ready flag */
#define orxDISPLAY_KU32_STATIC_FLAG_VSYNC       0x00000002 /**< VSync flag */
#define orxDISPLAY_KU32_STATIC_FLAG_SHADER      0x00000004 /**< Shader support flag */
#define orxDISPLAY_KU32_STATIC_FLAG_FOCUS       0x00000008 /**< Focus flag */
#define orxDISPLAY_KU32_STATIC_FLAG_NPOT        0x00000010 /**< NPOT texture support flag */
#define orxDISPLAY_KU32_STATIC_FLAG_EXT_READY   0x00000020 /**< Extensions ready flag */
#define orxDISPLAY_KU32_STATIC_FLAG_FRAMEBUFFER 0x00000040 /**< Framebuffer support flag */

#define orxDISPLAY_KU32_STATIC_MASK_ALL         0xFFFFFFFF /**< All mask */

#define orxDISPLAY_KU32_SCREEN_WIDTH            800
#define orxDISPLAY_KU32_SCREEN_HEIGHT           600
#define orxDISPLAY_KU32_SCREEN_DEPTH            32

#define orxDISPLAY_KU32_BITMAP_BANK_SIZE        256
#define orxDISPLAY_KU32_SHADER_BANK_SIZE        64

#define orxDISPLAY_KU32_BUFFER_SIZE             (12 * 1024)
#define orxDISPLAY_KU32_SHADER_BUFFER_SIZE      65536

#define orxDISPLAY_KF_BORDER_FIX                0.1f


/**  Misc defines
 */
#ifdef __orxDEBUG__

#define glASSERT()                                                        \
do                                                                        \
{                                                                         \
  /* Is window still opened? */                                           \
  if(glfwGetWindowParam(GLFW_OPENED) != GL_FALSE)                         \
  {                                                                       \
    GLenum eError = glGetError();                                         \
    orxASSERT(eError == GL_NO_ERROR && "OpenGL error code: 0x%X", eError);\
  }                                                                       \
} while(orxFALSE)

#else /* __orxDEBUG__ */

#define glASSERT()                                                        \
do                                                                        \
{                                                                         \
  glGetError();                                                           \
} while(orxFALSE)

#endif /* __orxDEBUG__ */


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Internal bitmap structure
 */
struct __orxBITMAP_t
{
  GLuint                    uiTexture;
  orxBOOL                   bSmoothing;
  orxFLOAT                  fWidth, fHeight;
  orxU32                    u32RealWidth, u32RealHeight, u32Depth;
  orxFLOAT                  fRecRealWidth, fRecRealHeight;
  orxCOLOR                  stColor;
  orxAABOX                  stClip;
};

/** Internal texture info structure
 */
typedef struct __orxDISPLAY_TEXTURE_INFO_t
{
  GLint                     iLocation;
  const orxBITMAP          *pstBitmap;

} orxDISPLAY_TEXTURE_INFO;

/** Internal shader structure
 */
typedef struct __orxDISPLAY_SHADER_t
{
  GLhandleARB               hProgram;
  GLint                     iTextureCounter;
  orxBOOL                   bActive;
  orxBOOL                   bInitialized;
  orxSTRING                 zCode;
  orxDISPLAY_TEXTURE_INFO  *astTextureInfoList;

} orxDISPLAY_SHADER;

/** Static structure
 */
typedef struct __orxDISPLAY_STATIC_t
{
  orxBANK                  *pstBitmapBank;
  orxBANK                  *pstShaderBank;
  orxBOOL                   bDefaultSmoothing;
  orxBITMAP                *pstScreen;
  orxBITMAP                *pstDestinationBitmap;
  const orxBITMAP          *pstLastBitmap;
  orxU8                   **aau8BufferArray;
  orxS32                    s32BitmapCounter;
  orxS32                    s32ShaderCounter;
  orxDISPLAY_BLEND_MODE     eLastBlendMode;
  GLint                     iTextureUnitNumber;
  GLuint                    uiFrameBuffer;
  orxS32                    s32ActiveShaderCounter;
  orxU32                    u32GLFWFlags;
  orxU32                    u32Flags;
  GLfloat                   afVertexList[orxDISPLAY_KU32_BUFFER_SIZE];
  GLfloat                   afTextureCoordList[orxDISPLAY_KU32_BUFFER_SIZE];
  orxCHAR                   acShaderCodeBuffer[orxDISPLAY_KU32_SHADER_BUFFER_SIZE];

} orxDISPLAY_STATIC;


/***************************************************************************
 * Static variables                                                        *
 ***************************************************************************/

/** Static data
 */
static orxDISPLAY_STATIC sstDisplay;


/** Shader-related OpenGL extension functions
 */
#ifndef __orxMAC__

PFNGLCREATEPROGRAMOBJECTARBPROC     glCreateProgramObjectARB    = NULL;
PFNGLCREATESHADEROBJECTARBPROC      glCreateShaderObjectARB     = NULL;
PFNGLDELETEOBJECTARBPROC            glDeleteObjectARB           = NULL;
PFNGLSHADERSOURCEARBPROC            glShaderSourceARB           = NULL;
PFNGLCOMPILESHADERARBPROC           glCompileShaderARB          = NULL;
PFNGLATTACHOBJECTARBPROC            glAttachObjectARB           = NULL;
PFNGLLINKPROGRAMARBPROC             glLinkProgramARB            = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC    glGetObjectParameterivARB   = NULL;
PFNGLGETINFOLOGARBPROC              glGetInfoLogARB             = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC        glUseProgramObjectARB       = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC      glGetUniformLocationARB     = NULL;
PFNGLUNIFORM1FARBPROC               glUniform1fARB              = NULL;
PFNGLUNIFORM3FARBPROC               glUniform3fARB              = NULL;
PFNGLUNIFORM1IARBPROC               glUniform1iARB              = NULL;

PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffersEXT        = NULL;
PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffersEXT     = NULL;
PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebufferEXT        = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatusEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2DEXT   = NULL;

  #ifndef __orxLINUX__

PFNGLACTIVETEXTUREARBPROC           glActiveTextureARB          = NULL;

  #endif /* __orxLINUX__ */

#endif /* __orxMAC__ */


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

static void orxFASTCALL orxDisplay_GLFW_Update(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
  /* Has focus? */
  if((glfwGetWindowParam(GLFW_ACTIVE) != GL_FALSE)
  && (glfwGetWindowParam(GLFW_ICONIFIED) == GL_FALSE))
  {
    /* Didn't have focus before? */
    if(!orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FOCUS))
    {
      /* Sends focus gained event */
      orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_FOCUS_GAINED);

      /* Updates focus status */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FOCUS, orxDISPLAY_KU32_STATIC_FLAG_NONE);
    }
  }
  else
  {
    /* Had focus before? */
    if(orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FOCUS))
    {
      /* Sends focus lost event */
      orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_FOCUS_LOST);

      /* Updates focus status */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_FOCUS);
    }
  }

  /* Is window closed? */
  if(glfwGetWindowParam(GLFW_OPENED) == GL_FALSE)
  {
    /* Sends system close event */
    orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_CLOSE);
  }

  /* Done! */
  return;
}

static orxINLINE void orxDisplay_GLFW_InitExtensions()
{
#define orxDISPLAY_LOAD_EXTENSION_FUNCTION(TYPE, FN)  FN = (TYPE)glfwGetProcAddress(#FN);

  /* Not already initialized? */
  if(!orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_EXT_READY))
  {
    /* Supports frame buffer? */
    if(glfwExtensionSupported("GL_EXT_framebuffer_object") != GL_FALSE)
    {
#ifndef __orxMAC__

      /* Loads frame buffer extension functions */
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT);

#endif /* __orxMAC__ */

      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FRAMEBUFFER, orxDISPLAY_KU32_STATIC_FLAG_NONE);
    }
    else
    {
      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_FRAMEBUFFER);
    }

    /* Has NPOT texture support? */
    if(glfwExtensionSupported("GL_ARB_texture_non_power_of_two") != GL_FALSE)
    {
      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT, orxDISPLAY_KU32_STATIC_FLAG_NONE);
    }
    else
    {
      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_NPOT);
    }

    /* Can support shader? */
    if((glfwExtensionSupported("GL_ARB_shader_objects") != GL_FALSE)
    && (glfwExtensionSupported("GL_ARB_shading_language_100") != GL_FALSE)
    && (glfwExtensionSupported("GL_ARB_vertex_shader") != GL_FALSE)
    && (glfwExtensionSupported("GL_ARB_fragment_shader") != GL_FALSE))
    {
#ifndef __orxMAC__

      /* Loads related OpenGL extension functions */
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLCREATEPROGRAMOBJECTARBPROC, glCreateProgramObjectARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLCREATESHADEROBJECTARBPROC, glCreateShaderObjectARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLDELETEOBJECTARBPROC, glDeleteObjectARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLSHADERSOURCEARBPROC, glShaderSourceARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLCOMPILESHADERARBPROC, glCompileShaderARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLATTACHOBJECTARBPROC, glAttachObjectARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLLINKPROGRAMARBPROC, glLinkProgramARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLGETOBJECTPARAMETERIVARBPROC, glGetObjectParameterivARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLGETINFOLOGARBPROC, glGetInfoLogARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLUSEPROGRAMOBJECTARBPROC, glUseProgramObjectARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLGETUNIFORMLOCATIONARBPROC, glGetUniformLocationARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLUNIFORM1FARBPROC, glUniform1fARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLUNIFORM3FARBPROC, glUniform3fARB);
      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLUNIFORM1IARBPROC, glUniform1iARB);

  #ifndef __orxLINUX__

      orxDISPLAY_LOAD_EXTENSION_FUNCTION(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB);

  #endif /* __orxLINUX__ */

#endif /* __orxMAC__ */

      /* Gets max texture unit number */
      glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &(sstDisplay.iTextureUnitNumber));
      glASSERT();

      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_SHADER, orxDISPLAY_KU32_STATIC_FLAG_NONE);
    }
    else
    {
      /* Updates status flags */
      orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_SHADER);
    }

    /* Updates status flags */
    orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_EXT_READY, orxDISPLAY_KU32_STATIC_FLAG_NONE);
  }

  /* Done! */
  return;
}

static orxSTATUS orxFASTCALL orxDisplay_GLFW_CompileShader(orxDISPLAY_SHADER *_pstShader)
{
  static const orxCHAR *szVertexShaderSource =
    "void main()"
    "{"
    "  gl_TexCoord[0] = gl_MultiTexCoord0;"
    "  gl_Position    = ftransform();"
    "}";

  GLhandleARB hProgram, hVertexShader, hFragmentShader;
  GLint       iSuccess;
  orxSTATUS   eResult = orxSTATUS_FAILURE;

  /* Creates program */
  hProgram = glCreateProgramObjectARB();
  glASSERT();

  /* Creates vertex and fragment shaders */
  hVertexShader   = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
  glASSERT();
  hFragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
  glASSERT();

  /* Compiles shader objects */
  glShaderSourceARB(hVertexShader, 1, (const GLcharARB **)&szVertexShaderSource, NULL);
  glASSERT();
  glShaderSourceARB(hFragmentShader, 1, (const GLcharARB **)&(_pstShader->zCode), NULL);
  glASSERT();
  glCompileShaderARB(hVertexShader);
  glASSERT();
  glCompileShaderARB(hFragmentShader);
  glASSERT();

  /* Gets vertex shader compiling status */
  glGetObjectParameterivARB(hVertexShader, GL_OBJECT_COMPILE_STATUS_ARB, &iSuccess);
  glASSERT();

  /* Success? */
  if(iSuccess != GL_FALSE)
  {
    /* Gets fragment shader compiling status */
    glGetObjectParameterivARB(hFragmentShader, GL_OBJECT_COMPILE_STATUS_ARB, &iSuccess);
    glASSERT();

    /* Success? */
    if(iSuccess != GL_FALSE)
    {
      /* Attaches shader objects to program */
      glAttachObjectARB(hProgram, hVertexShader);
      glASSERT();
      glAttachObjectARB(hProgram, hFragmentShader);
      glASSERT();

      /* Deletes shader objects */
      glDeleteObjectARB(hVertexShader);
      glASSERT();
      glDeleteObjectARB(hFragmentShader);
      glASSERT();

      /* Links program */
      glLinkProgramARB(hProgram);
      glASSERT();

      /* Gets linking status */
      glGetObjectParameterivARB(hProgram, GL_OBJECT_LINK_STATUS_ARB, &iSuccess);
      glASSERT();

      /* Success? */
      if(iSuccess != GL_FALSE)
      {
        /* Updates shader */
        _pstShader->hProgram        = hProgram;
        _pstShader->iTextureCounter = 0;

        /* Updates result */
        eResult = orxSTATUS_SUCCESS;
      }
      else
      {
        orxCHAR acBuffer[1024];

        /* Gets log */
        glGetInfoLogARB(hProgram, 1024 * sizeof(orxCHAR), NULL, (GLcharARB *)acBuffer);
        glASSERT();

        /* Outputs log */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't link shader program:\n%s\n", acBuffer);

        /* Deletes program */
        glDeleteObjectARB(hProgram);
        glASSERT();
      }
    }
    else
    {
      orxCHAR acBuffer[1024];

      /* Gets log */
      glGetInfoLogARB(hFragmentShader, 1024 * sizeof(orxCHAR), NULL, (GLcharARB *)acBuffer);
      glASSERT();

      /* Outputs log */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't compile fragment shader:\n%s\n", acBuffer);

      /* Deletes shader objects & program */
      glDeleteObjectARB(hVertexShader);
      glASSERT();
      glDeleteObjectARB(hFragmentShader);
      glASSERT();
      glDeleteObjectARB(hProgram);
      glASSERT();
    }
  }
  else
  {
    orxCHAR acBuffer[1024];

    /* Gets log */
    glGetInfoLogARB(hVertexShader, 1024 * sizeof(orxCHAR), NULL, (GLcharARB *)acBuffer);
    glASSERT();

    /* Outputs log */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't compile vertex shader:\n%s\n", acBuffer);

    /* Deletes shader objects & program */
    glDeleteObjectARB(hVertexShader);
    glASSERT();
    glDeleteObjectARB(hFragmentShader);
    glASSERT();
    glDeleteObjectARB(hProgram);
    glASSERT();
  }

  /* Done! */
  return eResult;
}

static orxINLINE void orxDisplay_GLFW_InitShader(orxDISPLAY_SHADER *_pstShader)
{
  GLint i;

  /* Uses its program */
  glUseProgramObjectARB(_pstShader->hProgram);
  glASSERT();

  /* For all defined textures */
  for(i = 0; i < _pstShader->iTextureCounter; i++)
  {
    /* Updates corresponding texture unit */
    glUniform1iARB(_pstShader->astTextureInfoList[i].iLocation, i);
    glASSERT();
    glActiveTextureARB(GL_TEXTURE0_ARB + i);
    glASSERT();
    glBindTexture(GL_TEXTURE_2D, _pstShader->astTextureInfoList[i].pstBitmap->uiTexture);
    glASSERT();

    /* Screen? */
    if(_pstShader->astTextureInfoList[i].pstBitmap == sstDisplay.pstScreen)
    {
      /* Copies screen content */
      glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, orxF2U(sstDisplay.pstScreen->fHeight) - sstDisplay.pstScreen->u32RealHeight, sstDisplay.pstScreen->u32RealWidth, sstDisplay.pstScreen->u32RealHeight);
      glASSERT();
    }
  }

  /* Updates its status */
  _pstShader->bInitialized = orxTRUE;

  /* Done! */
  return;
}

static orxINLINE void orxDisplay_GLFW_PrepareBitmap(const orxBITMAP *_pstBitmap, orxDISPLAY_SMOOTHING _eSmoothing, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  orxBOOL bSmoothing;

  /* Checks */
  orxASSERT((_pstBitmap != orxNULL) && (_pstBitmap != sstDisplay.pstScreen));

  /* New bitmap? */
  if(_pstBitmap != sstDisplay.pstLastBitmap)
  {
    /* No active shader? */
    if(sstDisplay.s32ActiveShaderCounter == 0)
    {
      /* Binds source's texture */
      glBindTexture(GL_TEXTURE_2D, _pstBitmap->uiTexture);
      glASSERT();

      /* Stores it */
      sstDisplay.pstLastBitmap = _pstBitmap;
    }
    else
    {
      /* Clears last bitmap */
      sstDisplay.pstLastBitmap = orxNULL;
    }
  }

  /* Depending on smoothing type */
  switch(_eSmoothing)
  {
    case orxDISPLAY_SMOOTHING_ON:
    {
      /* Applies smoothing */
      bSmoothing = orxTRUE;

      break;
    }

    case orxDISPLAY_SMOOTHING_OFF:
    {
      /* Applies no smoothing */
      bSmoothing = orxFALSE;

      break;
    }

    default:
    case orxDISPLAY_SMOOTHING_DEFAULT:
    {
      /* Applies default smoothing */
      bSmoothing = sstDisplay.bDefaultSmoothing;

      break;
    }
  }

  /* Should update smoothing? */
  if(bSmoothing != _pstBitmap->bSmoothing)
  {
    /* Smoothing? */
    if(bSmoothing != orxFALSE)
    {
      /* Updates texture */
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glASSERT();

      /* Updates mode */
      ((orxBITMAP *)_pstBitmap)->bSmoothing = orxTRUE;
    }
    else
    {
      /* Updates texture */
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glASSERT();

      /* Updates mode */
      ((orxBITMAP *)_pstBitmap)->bSmoothing = orxFALSE;
    }
  }

  /* New blend mode? */
  if(_eBlendMode != sstDisplay.eLastBlendMode)
  {
    /* Stores it */
    sstDisplay.eLastBlendMode = _eBlendMode;

    /* Depending on blend mode */
    switch(_eBlendMode)
    {
      case orxDISPLAY_BLEND_MODE_ALPHA:
      {
        glEnable(GL_BLEND);
        glASSERT();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glASSERT();

        break;
      }

      case orxDISPLAY_BLEND_MODE_MULTIPLY:
      {
        glEnable(GL_BLEND);
        glASSERT();
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        glASSERT();

        break;
      }

      case orxDISPLAY_BLEND_MODE_ADD:
      {
        glEnable(GL_BLEND);
        glASSERT();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glASSERT();

        break;
      }

      default:
      {
        glDisable(GL_BLEND);
        glASSERT();

        break;
      }
    }
  }

  /* Applies color */
  glColor4f(_pstBitmap->stColor.vRGB.fR, _pstBitmap->stColor.vRGB.fG, _pstBitmap->stColor.vRGB.fB, _pstBitmap->stColor.fAlpha);
  glASSERT();

  /* Done! */
  return;
}

static orxINLINE void orxDisplay_GLFW_DrawBitmap(const orxBITMAP *_pstBitmap, orxDISPLAY_SMOOTHING _eSmoothing, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  GLfloat fWidth, fHeight;
  GLfloat afVertexList[8];
  GLfloat afTextureCoordList[8];

  /* Prepares bitmap for drawing */
  orxDisplay_GLFW_PrepareBitmap(_pstBitmap, _eSmoothing, _eBlendMode);

  /* Gets bitmap working size */
  fWidth  = (GLfloat)(_pstBitmap->stClip.vBR.fX - _pstBitmap->stClip.vTL.fX);
  fHeight = (GLfloat)(_pstBitmap->stClip.vBR.fY - _pstBitmap->stClip.vTL.fY);

  /* Fill the vertex list */
  afVertexList[0] = 0.0f;
  afVertexList[1] = fHeight;
  afVertexList[2] = 0.0f;
  afVertexList[3] = 0.0f;
  afVertexList[4] = fWidth;
  afVertexList[5] = fHeight;
  afVertexList[6] = fWidth;
  afVertexList[7] = 0.0f;
  
  /* Fill the texture coord list */
  afTextureCoordList[0] = (GLfloat)(_pstBitmap->fRecRealWidth * (_pstBitmap->stClip.vTL.fX + orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[1] = (GLfloat)(orxFLOAT_1 - _pstBitmap->fRecRealHeight * (_pstBitmap->stClip.vBR.fY - orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[2] = (GLfloat)(_pstBitmap->fRecRealWidth * (_pstBitmap->stClip.vTL.fX + orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[3] = (GLfloat)(orxFLOAT_1 - _pstBitmap->fRecRealHeight * (_pstBitmap->stClip.vTL.fY + orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[4] = (GLfloat)(_pstBitmap->fRecRealWidth * (_pstBitmap->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[5] = (GLfloat)(orxFLOAT_1 - _pstBitmap->fRecRealHeight * (_pstBitmap->stClip.vBR.fY - orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[6] = (GLfloat)(_pstBitmap->fRecRealWidth * (_pstBitmap->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
  afTextureCoordList[7] = (GLfloat)(orxFLOAT_1 - _pstBitmap->fRecRealHeight * (_pstBitmap->stClip.vTL.fY + orxDISPLAY_KF_BORDER_FIX));

  /* Selects arrays */
  glVertexPointer(2, GL_FLOAT, 0, afVertexList);
  glASSERT();
  glTexCoordPointer(2, GL_FLOAT, 0, afTextureCoordList);
  glASSERT();

  /* Has active shaders? */
  if(sstDisplay.s32ActiveShaderCounter > 0)
  {
    orxDISPLAY_SHADER *pstShader;

    /* For all shaders */
    for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
        pstShader != orxNULL;
        pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
    {
      /* Is active? */
      if(pstShader->bActive != orxFALSE)
      {
        /* Inits shader */
        orxDisplay_GLFW_InitShader(pstShader);

        /* Draws arrays */
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glASSERT();
      }
    }
  }
  else
  {
    /* Draws arrays */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glASSERT();
  }

  /* Done! */
  return;
}

orxBITMAP *orxFASTCALL orxDisplay_GLFW_GetScreen()
{
  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Done! */
  return sstDisplay.pstScreen;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_TransformText(const orxSTRING _zString, const orxBITMAP *_pstFont, const orxCHARACTER_MAP *_pstMap, const orxDISPLAY_TRANSFORM *_pstTransform, orxDISPLAY_SMOOTHING _eSmoothing, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  const orxCHAR  *pc;
  orxU32          u32CharacterCodePoint, u32Counter;
  GLfloat         fX, fY, fWidth, fHeight;
  orxSTATUS       eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_zString != orxNULL);
  orxASSERT(_pstFont != orxNULL);
  orxASSERT(_pstMap != orxNULL);
  orxASSERT(_pstTransform != orxNULL);

  /* Translates it */
  glTranslatef(_pstTransform->fDstX, _pstTransform->fDstY, 0.0f);
  glASSERT();

  /* Applies rotation */
  glRotatef(orxMATH_KF_RAD_TO_DEG * _pstTransform->fRotation, 0.0f, 0.0f, 1.0f);
  glASSERT();

  /* Applies scale */
  glScalef(_pstTransform->fScaleX, _pstTransform->fScaleY, 1.0f);
  glASSERT();

  /* Applies pivot translation */
  glTranslatef(-_pstTransform->fSrcX, -_pstTransform->fSrcY, 0.0f);
  glASSERT();

  /* Gets character's size */
  fWidth  = _pstMap->vCharacterSize.fX;
  fHeight = _pstMap->vCharacterSize.fY;

  /* Prepares font for drawing */
  orxDisplay_GLFW_PrepareBitmap(_pstFont, _eSmoothing, _eBlendMode);

  /* For all characters */
  for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_zString, &pc), u32Counter = 0, fX = fY = 0.0f;
      u32CharacterCodePoint != orxCHAR_NULL;
      u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc))
  {
    /* Depending on character */
    switch(u32CharacterCodePoint)
    {
      case orxCHAR_CR:
      {
        /* Half EOL? */
        if(*pc == orxCHAR_LF)
        {
          /* Updates pointer */
          pc++;
        }

        /* Fall through */
      }
  
      case orxCHAR_LF:
      {
        /* Updates Y position */
        fY += fHeight;

        /* Resets X position */
        fX = 0.0f;

        break;
      }

      default:
      {
        const orxCHARACTER_GLYPH *pstGlyph;

        /* Gets glyph from UTF-8 table */
        pstGlyph = (orxCHARACTER_GLYPH *)orxHashTable_Get(_pstMap->pstCharacterTable, u32CharacterCodePoint);

        /* Valid? */
        if(pstGlyph != orxNULL)
        {
          /* End of buffer? */
          if(u32Counter > orxDISPLAY_KU32_BUFFER_SIZE - 12)
          {
            /* Selects arrays */
            glVertexPointer(2, GL_FLOAT, 0, sstDisplay.afVertexList);
            glASSERT();
            glTexCoordPointer(2, GL_FLOAT, 0, sstDisplay.afTextureCoordList);
            glASSERT();

            /* Has active shaders? */
            if(sstDisplay.s32ActiveShaderCounter > 0)
            {
              orxDISPLAY_SHADER *pstShader;

              /* For all shaders */
              for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
                  pstShader != orxNULL;
                  pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
              {
                /* Is active? */
                if(pstShader->bActive != orxFALSE)
                {
                  /* Inits shader */
                  orxDisplay_GLFW_InitShader(pstShader);

                  /* Draws arrays */
                  glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
                  glASSERT();
                }
              }
            }
            else
            {
              /* Draws arrays */
              glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
              glASSERT();
            }

            /* Resets counter */
            u32Counter = 0;
          }

          /* Outputs vertices and texture coordinates */
          sstDisplay.afVertexList[u32Counter]       =
          sstDisplay.afVertexList[u32Counter + 2]   =
          sstDisplay.afVertexList[u32Counter + 4]   = fX;
          sstDisplay.afVertexList[u32Counter + 6]   =
          sstDisplay.afVertexList[u32Counter + 8]   =
          sstDisplay.afVertexList[u32Counter + 10]  = fX + fWidth;
          sstDisplay.afVertexList[u32Counter + 5]   =
          sstDisplay.afVertexList[u32Counter + 9]   =
          sstDisplay.afVertexList[u32Counter + 11]  = fY;
          sstDisplay.afVertexList[u32Counter + 1]   =
          sstDisplay.afVertexList[u32Counter + 3]   =
          sstDisplay.afVertexList[u32Counter + 7]   = fY + fHeight;
          
          sstDisplay.afTextureCoordList[u32Counter]       =
          sstDisplay.afTextureCoordList[u32Counter + 2]   =
          sstDisplay.afTextureCoordList[u32Counter + 4]   = (GLfloat)(_pstFont->fRecRealWidth * (pstGlyph->fX + orxDISPLAY_KF_BORDER_FIX));
          sstDisplay.afTextureCoordList[u32Counter + 6]   =
          sstDisplay.afTextureCoordList[u32Counter + 8]   =
          sstDisplay.afTextureCoordList[u32Counter + 10]  = (GLfloat)(_pstFont->fRecRealWidth * (pstGlyph->fX + fWidth - orxDISPLAY_KF_BORDER_FIX));
          sstDisplay.afTextureCoordList[u32Counter + 5]   =
          sstDisplay.afTextureCoordList[u32Counter + 9]   =
          sstDisplay.afTextureCoordList[u32Counter + 11]  = (GLfloat)(orxFLOAT_1 - _pstFont->fRecRealHeight * (pstGlyph->fY + orxDISPLAY_KF_BORDER_FIX));
          sstDisplay.afTextureCoordList[u32Counter + 1]   =
          sstDisplay.afTextureCoordList[u32Counter + 3]   =
          sstDisplay.afTextureCoordList[u32Counter + 7]   = (GLfloat)(orxFLOAT_1 - _pstFont->fRecRealHeight * (pstGlyph->fY + fHeight - orxDISPLAY_KF_BORDER_FIX));

          /* Updates counter */
          u32Counter += 12;
        }
      }

      /* Updates X position */
      fX += fWidth;
    }
  }

  /* Has remaining data? */
  if(u32Counter != 0)
  {
    /* Selects arrays */
    glVertexPointer(2, GL_FLOAT, 0, sstDisplay.afVertexList);
    glASSERT();
    glTexCoordPointer(2, GL_FLOAT, 0, sstDisplay.afTextureCoordList);
    glASSERT();

    /* Has active shaders? */
    if(sstDisplay.s32ActiveShaderCounter > 0)
    {
      orxDISPLAY_SHADER *pstShader;

      /* For all shaders */
      for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
          pstShader != orxNULL;
          pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
      {
        /* Is active? */
        if(pstShader->bActive != orxFALSE)
        {
          /* Inits shader */
          orxDisplay_GLFW_InitShader(pstShader);

          /* Draws arrays */
          glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
          glASSERT();
        }
      }
    }
    else
    {
      /* Draws arrays */
      glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
      glASSERT();
    }
  }

  /* Restores identity */
  glLoadIdentity();
  glASSERT();

  /* Done! */
  return eResult;
}

void orxFASTCALL orxDisplay_GLFW_DeleteBitmap(orxBITMAP *_pstBitmap)
{
  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);

  /* Not screen? */
  if(_pstBitmap != sstDisplay.pstScreen)
  {
    /* Deletes its texture */
    glDeleteTextures(1, &(_pstBitmap->uiTexture));
    glASSERT();

    /* Deletes it */
    orxBank_Free(sstDisplay.pstBitmapBank, _pstBitmap);
  }

  return;
}

orxBITMAP *orxFASTCALL orxDisplay_GLFW_CreateBitmap(orxU32 _u32Width, orxU32 _u32Height)
{
  orxBITMAP *pstBitmap;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Allocates bitmap */
  pstBitmap = (orxBITMAP *)orxBank_Allocate(sstDisplay.pstBitmapBank);

  /* Valid? */
  if(pstBitmap != orxNULL)
  {
    /* Pushes display section */
    orxConfig_PushSection(orxDISPLAY_KZ_CONFIG_SECTION);

    /* Inits it */
    pstBitmap->bSmoothing     = orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_SMOOTH);
    pstBitmap->fWidth         = orxU2F(_u32Width);
    pstBitmap->fHeight        = orxU2F(_u32Height);
    pstBitmap->u32RealWidth   = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? _u32Width : orxMath_GetNextPowerOfTwo(_u32Width);
    pstBitmap->u32RealHeight  = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? _u32Height : orxMath_GetNextPowerOfTwo(_u32Height);
    pstBitmap->u32Depth       = 32;
    pstBitmap->fRecRealWidth  = orxFLOAT_1 / orxU2F(pstBitmap->u32RealWidth);
    pstBitmap->fRecRealHeight = orxFLOAT_1 / orxU2F(pstBitmap->u32RealHeight);
    orxColor_Set(&(pstBitmap->stColor), &orxVECTOR_WHITE, orxFLOAT_1);
    orxVector_Copy(&(pstBitmap->stClip.vTL), &orxVECTOR_0);
    orxVector_Set(&(pstBitmap->stClip.vBR), pstBitmap->fWidth, pstBitmap->fHeight, orxFLOAT_0);

    /* Creates new texture */
    glGenTextures(1, &pstBitmap->uiTexture);
    glASSERT();
    glBindTexture(GL_TEXTURE_2D, pstBitmap->uiTexture);
    glASSERT();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pstBitmap->u32RealWidth, pstBitmap->u32RealHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (pstBitmap->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (pstBitmap->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
    glASSERT();

    /* Pops config section */
    orxConfig_PopSection();
  }

  /* Done! */
  return pstBitmap;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_ClearBitmap(orxBITMAP *_pstBitmap, orxRGBA _stColor)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);

  /* Is not screen? */
  if(_pstBitmap != sstDisplay.pstScreen)
  {
    orxRGBA *astBuffer, *pstPixel;

    /* Allocates buffer */
    astBuffer = (orxRGBA *)orxMemory_Allocate(_pstBitmap->u32RealWidth * _pstBitmap->u32RealHeight * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);

    /* Checks */
    orxASSERT(astBuffer != orxNULL);

    /* For all pixels */
    for(pstPixel = astBuffer; pstPixel < astBuffer + (_pstBitmap->u32RealWidth * _pstBitmap->u32RealHeight); pstPixel++)
    {
      /* Sets its value */
      *pstPixel = _stColor;
    }

    /* Binds texture */
    glBindTexture(GL_TEXTURE_2D, _pstBitmap->uiTexture);
    glASSERT();

    /* Updates texture */
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _pstBitmap->u32RealWidth, _pstBitmap->u32RealHeight, GL_RGBA, GL_UNSIGNED_BYTE, astBuffer);
    glASSERT();

    /* Frees buffer */
    orxMemory_Free(astBuffer);
  }
  else
  {
    /* Clears the color buffer with given color */
    glClearColor((1.0f / 255.f) * orxU2F(orxRGBA_R(_stColor)), (1.0f / 255.f) * orxU2F(orxRGBA_G(_stColor)), (1.0f / 255.f) * orxU2F(orxRGBA_B(_stColor)), (1.0f / 255.f) * orxU2F(orxRGBA_A(_stColor)));
    glASSERT();
    glClear(GL_COLOR_BUFFER_BIT);
    glASSERT();
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_Swap()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Swap buffers */
  glfwSwapBuffers();

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetBitmapData(orxBITMAP *_pstBitmap, const orxU8 *_au8Data, orxU32 _u32ByteNumber)
{
  orxU32    u32Width, u32Height;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);
  orxASSERT(_au8Data != orxNULL);

  /* Gets bitmap's size */
  u32Width  = orxF2U(_pstBitmap->fWidth);
  u32Height = orxF2U(_pstBitmap->fHeight);

  /* Valid? */
  if((_pstBitmap != sstDisplay.pstScreen) && (_u32ByteNumber == u32Width * u32Height * sizeof(orxRGBA)))
  {
    orxU8        *pu8ImageBuffer;
    orxU32        i, u32LineSize, u32RealLineSize, u32SrcOffset, u32DstOffset;

    /* Allocates buffer */
    pu8ImageBuffer = (orxU8 *)orxMemory_Allocate(_pstBitmap->u32RealWidth * _pstBitmap->u32RealHeight * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);

    /* Gets line sizes */
    u32LineSize     = orxF2U(_pstBitmap->fWidth) * sizeof(orxRGBA);
    u32RealLineSize = _pstBitmap->u32RealWidth * sizeof(orxRGBA);

    /* Clears padding */
    orxMemory_Zero(pu8ImageBuffer, u32RealLineSize * (_pstBitmap->u32RealHeight - orxF2U(_pstBitmap->fHeight)));

    /* For all lines */
    for(i = 0, u32SrcOffset = 0, u32DstOffset = u32RealLineSize * (_pstBitmap->u32RealHeight - 1);
        i < u32Height;
        i++, u32SrcOffset += u32LineSize, u32DstOffset -= u32RealLineSize)
    {
      /* Copies data */
      orxMemory_Copy(pu8ImageBuffer + u32DstOffset, _au8Data + u32SrcOffset, u32LineSize);

      /* Adds padding */
      orxMemory_Zero(pu8ImageBuffer + u32DstOffset + u32LineSize, u32RealLineSize - u32LineSize);
    }

    /* Binds texture */
    glBindTexture(GL_TEXTURE_2D, _pstBitmap->uiTexture);
    glASSERT();

    /* Updates its content */
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _pstBitmap->u32RealWidth, _pstBitmap->u32RealHeight, GL_RGBA, GL_UNSIGNED_BYTE, pu8ImageBuffer);
    glASSERT();

    /* Frees buffer */
    orxMemory_Free(pu8ImageBuffer);

    /* Updates result */
    eResult = orxSTATUS_SUCCESS;
  }
  else
  {
    /* Screen? */
    if(_pstBitmap == sstDisplay.pstScreen)
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Can't set bitmap data: can't use screen as destination bitmap.");
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Can't set bitmap data: format needs to be RGBA.");
    }

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetBitmapColorKey(orxBITMAP *_pstBitmap, orxRGBA _stColor, orxBOOL _bEnable)
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Not available */
  orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Not available on this platform!");

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetBitmapColor(orxBITMAP *_pstBitmap, orxRGBA _stColor)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);

  /* Not screen? */
  if(_pstBitmap != sstDisplay.pstScreen)
  {
    /* Stores it */
    orxColor_SetRGBA(&(_pstBitmap->stColor), _stColor);
  }

  /* Done! */
  return eResult;
}

orxRGBA orxFASTCALL orxDisplay_GLFW_GetBitmapColor(const orxBITMAP *_pstBitmap)
{
  orxRGBA stResult = 0;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);

  /* Not screen? */
  if(_pstBitmap != sstDisplay.pstScreen)
  {
    /* Updates result */
    stResult = orxColor_ToRGBA(&(_pstBitmap->stColor));
  }

  /* Done! */
  return stResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetDestinationBitmap(orxBITMAP *_pstBitmap)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Different destination bitmap? */
  if(_pstBitmap != sstDisplay.pstDestinationBitmap)
  {
    /* Stores it */
    sstDisplay.pstDestinationBitmap = _pstBitmap;

    /* Has framebuffer support? */
    if(orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FRAMEBUFFER))
    {
      /* Screen? */
      if(_pstBitmap == sstDisplay.pstScreen)
      {
        /* Unbinds frame buffer */
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glASSERT();
        glFlush();
        glASSERT();

        /* Updates result */
        eResult = (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;
        glASSERT();
      }
      else
      {
        /* Binds frame buffer */
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, sstDisplay.uiFrameBuffer);
        glASSERT();

        /* Links it to frame buffer */  
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _pstBitmap->uiTexture, 0);
        glASSERT();

        /* Updates result */
        eResult = (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;
        glASSERT();
      }
    }
    else
    {
      /* Not screen? */
      if(_pstBitmap != sstDisplay.pstScreen)
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Can't set bitmap <0x%X> as destination bitmap: only screen can be used as frame buffer isn't supported by this hardware.", _pstBitmap);

        /* Updates result */
        eResult = orxSTATUS_FAILURE;
      }
    }

    /* Success? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Inits viewport */
      glViewport(0, 0, (GLsizei)orxF2S(sstDisplay.pstDestinationBitmap->fWidth), (GLsizei)orxF2S(sstDisplay.pstDestinationBitmap->fHeight));
      glASSERT();

      /* Inits matrices */
      glMatrixMode(GL_PROJECTION);
      glASSERT();
      glLoadIdentity();
      glASSERT();
      glOrtho(0.0f, sstDisplay.pstDestinationBitmap->fWidth, sstDisplay.pstDestinationBitmap->fHeight, 0.0f, -1.0f, 1.0f);
      glASSERT();
      glMatrixMode(GL_MODELVIEW);
      glASSERT();
      glLoadIdentity();
      glASSERT();
    }
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_BlitBitmap(const orxBITMAP *_pstSrc, const orxFLOAT _fPosX, orxFLOAT _fPosY, orxDISPLAY_SMOOTHING _eSmoothing, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_pstSrc != orxNULL) && (_pstSrc != sstDisplay.pstScreen));

  /* Translates it */
  glTranslatef(_fPosX, _fPosY, 0.0f);
  glASSERT();

  /* Draws it */
  orxDisplay_GLFW_DrawBitmap(_pstSrc, _eSmoothing, _eBlendMode);

  /* Restores identity */
  glLoadIdentity();
  glASSERT();

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_TransformBitmap(const orxBITMAP *_pstSrc, const orxDISPLAY_TRANSFORM *_pstTransform, orxDISPLAY_SMOOTHING _eSmoothing, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_pstSrc != orxNULL) && (_pstSrc != sstDisplay.pstScreen));
  orxASSERT(_pstTransform != orxNULL);

  /* Translates it */
  glTranslatef(_pstTransform->fDstX, _pstTransform->fDstY, 0.0f);
  glASSERT();

  /* Applies rotation */
  glRotatef(orxMATH_KF_RAD_TO_DEG * _pstTransform->fRotation, 0.0f, 0.0f, 1.0f);
  glASSERT();

  /* Applies scale */
  glScalef(_pstTransform->fScaleX, _pstTransform->fScaleY, 1.0f);
  glASSERT();

  /* Applies pivot translation */
  glTranslatef(-_pstTransform->fSrcX, -_pstTransform->fSrcY, 0.0f);
  glASSERT();

  /* No repeat? */
  if((_pstTransform->fRepeatX == orxFLOAT_1) && (_pstTransform->fRepeatY == orxFLOAT_1))
  {
    /* Draws it */
    orxDisplay_GLFW_DrawBitmap(_pstSrc, _eSmoothing, _eBlendMode);
  }
  else
  {
    orxFLOAT  i, j, fRecRepeatX;
    GLfloat   fX, fY, fWidth, fHeight, fTop, fBottom, fLeft, fRight;
    orxU32    u32Counter;

    /* Prepares bitmap for drawing */
    orxDisplay_GLFW_PrepareBitmap(_pstSrc, _eSmoothing, _eBlendMode);

    /* Inits bitmap height */
    fHeight = (GLfloat)((_pstSrc->stClip.vBR.fY - _pstSrc->stClip.vTL.fY) / _pstTransform->fRepeatY);

    /* Inits texture coords */
    fLeft   = _pstSrc->fRecRealWidth * (_pstSrc->stClip.vTL.fX + orxDISPLAY_KF_BORDER_FIX);
    fTop    = orxFLOAT_1 - _pstSrc->fRecRealHeight * (_pstSrc->stClip.vTL.fY + orxDISPLAY_KF_BORDER_FIX);

    /* For all lines */
    for(fY = 0.0f, i = _pstTransform->fRepeatY, u32Counter = 0, fRecRepeatX = orxFLOAT_1 / _pstTransform->fRepeatX; i > orxFLOAT_0; i -= orxFLOAT_1, fY += fHeight)
    {
      /* Partial line? */
      if(i < orxFLOAT_1)
      {
        /* Updates height */
        fHeight *= (GLfloat)i;

        /* Resets texture coords */
        fRight  = (GLfloat)(_pstSrc->fRecRealWidth * (_pstSrc->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
        fBottom = (GLfloat)(orxFLOAT_1 - _pstSrc->fRecRealHeight * (_pstSrc->stClip.vTL.fY + (i * (_pstSrc->stClip.vBR.fY - _pstSrc->stClip.vTL.fY)) - orxDISPLAY_KF_BORDER_FIX));
      }
      else
      {
        /* Resets texture coords */
        fRight  = (GLfloat)(_pstSrc->fRecRealWidth * (_pstSrc->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
        fBottom = (GLfloat)(orxFLOAT_1 - _pstSrc->fRecRealHeight * (_pstSrc->stClip.vBR.fY - orxDISPLAY_KF_BORDER_FIX));
      }

      /* Resets bitmap width */
      fWidth = (GLfloat)((_pstSrc->stClip.vBR.fX - _pstSrc->stClip.vTL.fX) * fRecRepeatX);

      /* For all columns */
      for(fX = 0.0f, j = _pstTransform->fRepeatX; j > orxFLOAT_0; j -= orxFLOAT_1, fX += fWidth)
      {
        /* End of buffer? */
        if(u32Counter > orxDISPLAY_KU32_BUFFER_SIZE - 12)
        {
          /* Selects arrays */
          glVertexPointer(2, GL_FLOAT, 0, sstDisplay.afVertexList);
          glASSERT();
          glTexCoordPointer(2, GL_FLOAT, 0, sstDisplay.afTextureCoordList);
          glASSERT();

          /* Has active shaders? */
          if(sstDisplay.s32ActiveShaderCounter > 0)
          {
            orxDISPLAY_SHADER *pstShader;

            /* For all shaders */
            for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
                pstShader != orxNULL;
                pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
            {
              /* Is active? */
              if(pstShader->bActive != orxFALSE)
              {
                /* Inits shader */
                orxDisplay_GLFW_InitShader(pstShader);

                /* Draws arrays */
                glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
                glASSERT();
              }
            }
          }
          else
          {
            /* Draws arrays */
            glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
            glASSERT();
          }

          /* Resets counter */
          u32Counter = 0;
        }

        /* Partial column? */
        if(j < orxFLOAT_1)
        {
          /* Updates width */
          fWidth *= (GLfloat)j;

          /* Updates texture right coord */
          fRight = (GLfloat)(_pstSrc->fRecRealWidth * (_pstSrc->stClip.vTL.fX + (j * (_pstSrc->stClip.vBR.fX - _pstSrc->stClip.vTL.fX))));
        }

        /* Outputs vertices and texture coordinates */
        sstDisplay.afVertexList[u32Counter]       =
        sstDisplay.afVertexList[u32Counter + 2]   =
        sstDisplay.afVertexList[u32Counter + 4]   = fX;
        sstDisplay.afVertexList[u32Counter + 6]   =
        sstDisplay.afVertexList[u32Counter + 8]   =
        sstDisplay.afVertexList[u32Counter + 10]  = fX + fWidth;
        sstDisplay.afVertexList[u32Counter + 5]   =
        sstDisplay.afVertexList[u32Counter + 9]   =
        sstDisplay.afVertexList[u32Counter + 11]  = fY;
        sstDisplay.afVertexList[u32Counter + 1]   =
        sstDisplay.afVertexList[u32Counter + 3]   =
        sstDisplay.afVertexList[u32Counter + 7]   = fY + fHeight;

        sstDisplay.afTextureCoordList[u32Counter]       =
        sstDisplay.afTextureCoordList[u32Counter + 2]   =
        sstDisplay.afTextureCoordList[u32Counter + 4]   = fLeft;
        sstDisplay.afTextureCoordList[u32Counter + 6]   =
        sstDisplay.afTextureCoordList[u32Counter + 8]   =
        sstDisplay.afTextureCoordList[u32Counter + 10]  = fRight;
        sstDisplay.afTextureCoordList[u32Counter + 5]   =
        sstDisplay.afTextureCoordList[u32Counter + 9]   =
        sstDisplay.afTextureCoordList[u32Counter + 11]  = fTop;
        sstDisplay.afTextureCoordList[u32Counter + 1]   =
        sstDisplay.afTextureCoordList[u32Counter + 3]   =
        sstDisplay.afTextureCoordList[u32Counter + 7]   = fBottom;

        /* Updates counter */
        u32Counter += 12;
      }
    }

    /* Has remaining data? */
    if(u32Counter != 0)
    {
      /* Selects arrays */
      glVertexPointer(2, GL_FLOAT, 0, sstDisplay.afVertexList);
      glASSERT();
      glTexCoordPointer(2, GL_FLOAT, 0, sstDisplay.afTextureCoordList);
      glASSERT();

      /* Has active shaders? */
      if(sstDisplay.s32ActiveShaderCounter > 0)
      {
        orxDISPLAY_SHADER *pstShader;

        /* For all shaders */
        for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
            pstShader != orxNULL;
            pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
        {
          /* Is active? */
          if(pstShader->bActive != orxFALSE)
          {
            /* Inits shader */
            orxDisplay_GLFW_InitShader(pstShader);

            /* Draws arrays */
            glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
            glASSERT();
          }
        }
      }
      else
      {
        /* Draws arrays */
        glDrawArrays(GL_TRIANGLE_STRIP, 0, u32Counter >> 1);
        glASSERT();
      }
    }
  }

  /* Restores identity */
  glLoadIdentity();
  glASSERT();

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SaveBitmap(const orxBITMAP *_pstBitmap, const orxSTRING _zFilename)
{
  int             iFormat;
  orxU32          u32Length, u32LineSize, u32RealLineSize, u32SrcOffset, u32DstOffset, i;
  const orxCHAR  *zExtension;
  orxU8          *pu8ImageBuffer, *pu8ImageData;
  orxSTATUS       eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);
  orxASSERT(_zFilename != orxNULL);

  /* Gets file name's length */
  u32Length = orxString_GetLength(_zFilename);

  /* Gets extension */
  zExtension = (u32Length > 3) ? _zFilename + u32Length - 3 : orxSTRING_EMPTY;

  /* DDS? */
  if(orxString_ICompare(zExtension, "dds") == 0)
  {
    /* Updates format */
    iFormat = SOIL_SAVE_TYPE_DDS;
  }
  /* BMP? */
  else if(orxString_ICompare(zExtension, "bmp") == 0)
  {
    /* Updates format */
    iFormat = SOIL_SAVE_TYPE_BMP;
  }
  /* TGA */
  else
  {
    /* Updates format */
    iFormat = SOIL_SAVE_TYPE_TGA;
  }

  /* Allocates buffers */
  pu8ImageData    = (orxU8 *)orxMemory_Allocate(_pstBitmap->u32RealWidth * _pstBitmap->u32RealHeight * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);
  pu8ImageBuffer  = (orxU8 *)orxMemory_Allocate(orxF2U(_pstBitmap->fWidth * _pstBitmap->fHeight) * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);

  /* Checks */
  orxASSERT(pu8ImageData != orxNULL);
  orxASSERT(pu8ImageBuffer != orxNULL);

  /* Binds its assocaited texture */
  glBindTexture(GL_TEXTURE_2D, _pstBitmap->uiTexture);
  glASSERT();

  /* Screen capture? */
  if(_pstBitmap == sstDisplay.pstScreen)
  {
    /* Isn't screen the current destination? */
    if(sstDisplay.pstScreen != sstDisplay.pstDestinationBitmap)
    {
      /* Sets it as destination */
      orxDisplay_GLFW_SetDestinationBitmap(sstDisplay.pstScreen);
    }

    /* Copies screen content */
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, orxF2U(_pstBitmap->fHeight) - _pstBitmap->u32RealHeight, _pstBitmap->u32RealWidth, _pstBitmap->u32RealHeight);
    glASSERT();
  }

  /* Copies bitmap data */
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pu8ImageData);
  glASSERT();

  /* Gets line sizes */
  u32LineSize     = orxF2U(_pstBitmap->fWidth) * sizeof(orxRGBA);
  u32RealLineSize = _pstBitmap->u32RealWidth * sizeof(orxRGBA);

  /* Clears padding */
  orxMemory_Zero(pu8ImageBuffer, u32LineSize * orxF2U(_pstBitmap->fHeight));

  /* For all lines */
  for(i = 0, u32SrcOffset = u32RealLineSize * (_pstBitmap->u32RealHeight - orxF2U(_pstBitmap->fHeight)), u32DstOffset = u32LineSize * (orxF2U(_pstBitmap->fHeight) - 1);
    i < orxF2U(_pstBitmap->fHeight);
    i++, u32SrcOffset += u32RealLineSize, u32DstOffset -= u32LineSize)
  {
    /* Copies data */
    orxMemory_Copy(pu8ImageBuffer + u32DstOffset, pu8ImageData + u32SrcOffset, u32LineSize);
  }

  /* Saves screenshot */
  eResult = SOIL_save_image(_zFilename, iFormat, orxF2U(_pstBitmap->fWidth), orxF2U(_pstBitmap->fHeight), 4, pu8ImageBuffer) != 0 ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Frees buffers */
  orxMemory_Free(pu8ImageBuffer);
  orxMemory_Free(pu8ImageData);

  /* Done! */
  return eResult;
}

orxBITMAP *orxFASTCALL orxDisplay_GLFW_LoadBitmap(const orxSTRING _zFilename)
{
  unsigned char  *pu8ImageData;
  GLuint          uiWidth, uiHeight, uiBytesPerPixel;
  orxBITMAP      *pstResult = orxNULL;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Loads image */
  pu8ImageData = SOIL_load_image(_zFilename, (int *)&uiWidth, (int *)&uiHeight, (int *)&uiBytesPerPixel, SOIL_LOAD_RGBA);

  /* Valid? */
  if(pu8ImageData != NULL)
  {
    /* Allocates bitmap */
    pstResult = (orxBITMAP *)orxBank_Allocate(sstDisplay.pstBitmapBank);

    /* Valid? */
    if(pstResult != orxNULL)
    {
      GLuint  i, uiSrcOffset, uiDstOffset, uiLineSize, uiRealLineSize;
      orxU8  *pu8ImageBuffer;
      GLuint  uiRealWidth, uiRealHeight;

      /* Gets its real size */
      uiRealWidth   = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? uiWidth : orxMath_GetNextPowerOfTwo(uiWidth);
      uiRealHeight  = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? uiHeight : orxMath_GetNextPowerOfTwo(uiHeight);
   
      /* Pushes display section */
      orxConfig_PushSection(orxDISPLAY_KZ_CONFIG_SECTION);

      /* Inits bitmap */
      pstResult->bSmoothing     = orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_SMOOTH);
      pstResult->fWidth         = orxU2F(uiWidth);
      pstResult->fHeight        = orxU2F(uiHeight);
      pstResult->u32RealWidth   = uiRealWidth;
      pstResult->u32RealHeight  = uiRealHeight;
      pstResult->u32Depth       = 32;
      pstResult->fRecRealWidth  = orxFLOAT_1 / orxU2F(pstResult->u32RealWidth);
      pstResult->fRecRealHeight = orxFLOAT_1 / orxU2F(pstResult->u32RealHeight);
      orxColor_Set(&(pstResult->stColor), &orxVECTOR_WHITE, orxFLOAT_1);
      orxVector_Copy(&(pstResult->stClip.vTL), &orxVECTOR_0);
      orxVector_Set(&(pstResult->stClip.vBR), pstResult->fWidth, pstResult->fHeight, orxFLOAT_0);
 
      /* Allocates buffer */
      pu8ImageBuffer = (orxU8 *)orxMemory_Allocate(uiRealWidth * uiRealHeight * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);

      /* Checks */
      orxASSERT(pu8ImageBuffer != orxNULL);

      /* Gets line sizes */
      uiLineSize      = uiWidth * sizeof(orxRGBA);
      uiRealLineSize  = uiRealWidth * sizeof(orxRGBA);

      /* Clears padding */
      orxMemory_Zero(pu8ImageBuffer, uiRealLineSize * (uiRealHeight - uiHeight));

      /* For all lines */
      for(i = 0, uiSrcOffset = 0, uiDstOffset = uiRealLineSize * (uiRealHeight - 1);
          i < uiHeight;
          i++, uiSrcOffset += uiLineSize, uiDstOffset -= uiRealLineSize)
      {
        /* Copies data */
        orxMemory_Copy(pu8ImageBuffer + uiDstOffset, pu8ImageData + uiSrcOffset, uiLineSize);

        /* Adds padding */
        orxMemory_Zero(pu8ImageBuffer + uiDstOffset + uiLineSize, uiRealLineSize - uiLineSize);
      }

      /* Creates new texture */
      glGenTextures(1, &pstResult->uiTexture);
      glASSERT();
      glBindTexture(GL_TEXTURE_2D, pstResult->uiTexture);
      glASSERT();
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pstResult->u32RealWidth, pstResult->u32RealHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pu8ImageBuffer);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (pstResult->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
      glASSERT();
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (pstResult->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
      glASSERT();

      /* Frees image buffer */
      orxMemory_Free(pu8ImageBuffer);

      /* Pops config section */
      orxConfig_PopSection();
    }

    /* Deletes surface */
    SOIL_free_image_data(pu8ImageData);
  }

  /* Done! */
  return pstResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_GetBitmapSize(const orxBITMAP *_pstBitmap, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Gets size */
  *_pfWidth   = _pstBitmap->fWidth;
  *_pfHeight  = _pstBitmap->fHeight;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_GetScreenSize(orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Gets size */
  *_pfWidth   = sstDisplay.pstScreen->fWidth;
  *_pfHeight  = sstDisplay.pstScreen->fHeight;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetBitmapClipping(orxBITMAP *_pstBitmap, orxU32 _u32TLX, orxU32 _u32TLY, orxU32 _u32BRX, orxU32 _u32BRY)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstBitmap != orxNULL);

  /* Destination bitmap? */
  if(_pstBitmap == sstDisplay.pstDestinationBitmap)
  {
    /* Enables clipping */
    glEnable(GL_SCISSOR_TEST);
    glASSERT();

    /* Stores screen clipping */
    glScissor(_u32TLX, orxF2U(sstDisplay.pstDestinationBitmap->fHeight) - _u32BRY, _u32BRX - _u32TLX, _u32BRY - _u32TLY);
    glASSERT();
  }

  /* Stores clip coords */
  orxVector_Set(&(_pstBitmap->stClip.vTL), orxU2F(_u32TLX), orxU2F(_u32TLY), orxFLOAT_0);
  orxVector_Set(&(_pstBitmap->stClip.vBR), orxU2F(_u32BRX), orxU2F(_u32BRY), orxFLOAT_0);

  /* Done! */
  return eResult;
}

orxU32 orxFASTCALL orxDisplay_GLFW_GetVideoModeCounter()
{
  GLFWvidmode astModeList[64];
  orxU32      u32Result = 0;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Gets video mode list */
  u32Result = (orxU32)glfwGetVideoModes(astModeList, 64);

  /* Done! */
  return u32Result;
}

orxDISPLAY_VIDEO_MODE *orxFASTCALL orxDisplay_GLFW_GetVideoMode(orxU32 _u32Index, orxDISPLAY_VIDEO_MODE *_pstVideoMode)
{
  GLFWvidmode             astModeList[64];
  orxU32                  u32Counter;
  orxDISPLAY_VIDEO_MODE  *pstResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstVideoMode != orxNULL);

  /* Gets video mode list */
  u32Counter = (orxU32)glfwGetVideoModes(astModeList, 64);

  /* Is index valid? */
  if(_u32Index < u32Counter)
  {
    /* Stores info */
    _pstVideoMode->u32Width   = astModeList[_u32Index].Width;
    _pstVideoMode->u32Height  = astModeList[_u32Index].Height;
    _pstVideoMode->u32Depth   = astModeList[_u32Index].RedBits + astModeList[_u32Index].GreenBits + astModeList[_u32Index].BlueBits;

    /* 24-bit? */
    if(_pstVideoMode->u32Depth == 24)
    {
      /* Gets 32-bit instead */
      _pstVideoMode->u32Depth = 32;
    }

    /* Updates result */
    pstResult = _pstVideoMode;
  }
  else
  {
    /* Updates result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

orxBOOL orxFASTCALL orxDisplay_GLFW_IsVideoModeAvailable(const orxDISPLAY_VIDEO_MODE *_pstVideoMode)
{
  GLFWvidmode astModeList[64];
  orxU32      u32Counter, i;
  orxBOOL     bResult = orxFALSE;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstVideoMode != orxNULL);

  /* Gets video mode list */
  u32Counter = (orxU32)glfwGetVideoModes(astModeList, 64);

  /* For all mode */
  for(i = 0; i < u32Counter; i++)
  {
    /* Matches? */
    if((_pstVideoMode->u32Width == (orxU32)astModeList[i].Width)
    && (_pstVideoMode->u32Height == (orxU32)astModeList[i].Height)
    && ((_pstVideoMode->u32Depth == (orxU32)(astModeList[i].RedBits + astModeList[i].GreenBits + astModeList[i].BlueBits))
     || ((_pstVideoMode->u32Depth == 32)
      && (astModeList[i].RedBits + astModeList[i].GreenBits + astModeList[i].BlueBits == 24))))
    {
      /* Updates result */
      bResult = orxTRUE;

      break;
    }
  }

  /* Done! */
  return bResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_EnableVSync(orxBOOL _bEnable)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Enable? */
  if(_bEnable != orxFALSE)
  {
    /* Updates VSync status */
    glfwSwapInterval(1);

    /* Updates status */
    orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_VSYNC, orxDISPLAY_KU32_STATIC_FLAG_NONE);
  }
  else
  {
    /* Updates VSync status */
    glfwSwapInterval(0);

    /* Updates status */
    orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_VSYNC);
  }

  /* Done! */
  return eResult;
}

orxBOOL orxFASTCALL orxDisplay_GLFW_IsVSyncEnabled()
{
  orxBOOL bResult = orxFALSE;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Updates result */
  bResult = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_VSYNC) ? orxTRUE : orxFALSE;

  /* Done! */
  return bResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetVideoMode(const orxDISPLAY_VIDEO_MODE *_pstVideoMode)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstVideoMode != orxNULL);

  /* Has opened window? */
  if(glfwGetWindowParam(GLFW_OPENED) != GL_FALSE)
  {
    /* Gets bitmap counter */
    sstDisplay.s32BitmapCounter = (orxS32)orxBank_GetCounter(sstDisplay.pstBitmapBank) - 1;

    /* Valid? */
    if(sstDisplay.s32BitmapCounter > 0)
    {
      orxBITMAP  *pstBitmap;
      orxU32      u32Index;

      /* Allocates buffer array */
      sstDisplay.aau8BufferArray = (orxU8 **)orxMemory_Allocate(sstDisplay.s32BitmapCounter * sizeof(orxU8 *), orxMEMORY_TYPE_MAIN);

      /* Checks */
      orxASSERT(sstDisplay.aau8BufferArray != orxNULL);

      /* For all bitmaps */
      for(pstBitmap = (orxBITMAP *)orxBank_GetNext(sstDisplay.pstBitmapBank, orxNULL), u32Index = 0;
          pstBitmap != orxNULL;
          pstBitmap = (orxBITMAP *)orxBank_GetNext(sstDisplay.pstBitmapBank, pstBitmap))
      {
        /* Not screen? */
        if(pstBitmap != sstDisplay.pstScreen)
        {
          /* Allocates its buffer */
          sstDisplay.aau8BufferArray[u32Index] = (orxU8 *)orxMemory_Allocate(pstBitmap->u32RealWidth * pstBitmap->u32RealHeight * sizeof(orxRGBA), orxMEMORY_TYPE_VIDEO);

          /* Checks */
          orxASSERT(sstDisplay.aau8BufferArray[u32Index] != orxNULL);

          /* Binds bitmap */
          glBindTexture(GL_TEXTURE_2D, pstBitmap->uiTexture);
          glASSERT();

          /* Copies bitmap data */
          glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, sstDisplay.aau8BufferArray[u32Index++]);
          glASSERT();

          /* Deletes it */
          glDeleteTextures(1, &(pstBitmap->uiTexture));
          glASSERT();
        }
      }
    }

    /* Gets shader counter */
    sstDisplay.s32ShaderCounter = (orxS32)orxBank_GetCounter(sstDisplay.pstShaderBank);

    /* Valid? */
    if(sstDisplay.s32ShaderCounter > 0)
    {
      orxDISPLAY_SHADER *pstShader;

      /* For all shaders */
      for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
          pstShader != orxNULL;
          pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
      {
        /* Deletes its program */
        glDeleteObjectARB(pstShader->hProgram);
        glASSERT();
        pstShader->hProgram = (GLhandleARB)orxHANDLE_UNDEFINED;
      }
    }

    /* Had frame buffer? */
    if(sstDisplay.uiFrameBuffer)
    {
      /* Deletes it */
      glDeleteFramebuffersEXT(1, &sstDisplay.uiFrameBuffer);
      sstDisplay.uiFrameBuffer = 0;
    }

    /* Closes window */
    glfwCloseWindow();
  }

  /* Updates window hint */
  glfwOpenWindowHint(GLFW_WINDOW_NO_RESIZE, GL_TRUE);

  /* Depending on video depth */
  switch(_pstVideoMode->u32Depth)
  {
    /* 16-bit */
    case 16:
    {
      /* Updates video mode */
      eResult = (glfwOpenWindow((int)_pstVideoMode->u32Width, (int)_pstVideoMode->u32Height, 5, 6, 5, 0, 0, 0, sstDisplay.u32GLFWFlags) != GL_FALSE) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

      break;
    }

    /* 24-bit */
    case 24:
    {
      /* Updates video mode */
      eResult = (glfwOpenWindow((int)_pstVideoMode->u32Width, (int)_pstVideoMode->u32Height, 8, 8, 8, 0, 0, 0, sstDisplay.u32GLFWFlags) != GL_FALSE) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

      break;
    }

    /* 32-bit */
    default:
    case 32:
    {
      /* Updates video mode */
      eResult = (glfwOpenWindow((int)_pstVideoMode->u32Width, (int)_pstVideoMode->u32Height, 8, 8, 8, 8, 0, 0, sstDisplay.u32GLFWFlags) != GL_FALSE) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

      break;
    }
  }

  /* Success? */
  if(eResult != orxSTATUS_FAILURE)
  {
    /* Pushes display section */
    orxConfig_PushSection(orxDISPLAY_KZ_CONFIG_SECTION);

    /* Updates its title */
    glfwSetWindowTitle(orxConfig_GetString(orxDISPLAY_KZ_CONFIG_TITLE));

    /* Pops config section */
    orxConfig_PopSection();

    /* Inits extensions */
    orxDisplay_GLFW_InitExtensions();

    /* Inits OpenGL */
    glEnable(GL_TEXTURE_2D);
    glASSERT();
    glDisable(GL_LIGHTING);
    glASSERT();
    glDisable(GL_FOG);
    glASSERT();
    glDisable(GL_CULL_FACE);
    glASSERT();
    glDisable(GL_DEPTH_TEST);
    glASSERT();
    glDisable(GL_STENCIL_TEST);
    glASSERT();
    glEnableClientState(GL_VERTEX_ARRAY);
    glASSERT();
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glASSERT();

    /* Has framebuffer support? */
    if(orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_FRAMEBUFFER))
    {
      /* Generates frame buffer */
      glGenFramebuffersEXT(1, &sstDisplay.uiFrameBuffer);
    }

    /* Updates screen info */
    sstDisplay.pstScreen->fWidth          = orx2F(_pstVideoMode->u32Width);
    sstDisplay.pstScreen->fHeight         = orx2F(_pstVideoMode->u32Height);
    sstDisplay.pstScreen->u32RealWidth    = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? _pstVideoMode->u32Width : orxMath_GetNextPowerOfTwo(_pstVideoMode->u32Width);
    sstDisplay.pstScreen->u32RealHeight   = orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NPOT) ? _pstVideoMode->u32Height : orxMath_GetNextPowerOfTwo(_pstVideoMode->u32Height);
    sstDisplay.pstScreen->u32Depth        = _pstVideoMode->u32Depth;
    sstDisplay.pstScreen->bSmoothing      = sstDisplay.bDefaultSmoothing;
    sstDisplay.pstScreen->fRecRealWidth   = orxFLOAT_1 / orxU2F(sstDisplay.pstScreen->u32RealWidth);
    sstDisplay.pstScreen->fRecRealHeight  = orxFLOAT_1 / orxU2F(sstDisplay.pstScreen->u32RealHeight);
    orxVector_Copy(&(sstDisplay.pstScreen->stClip.vTL), &orxVECTOR_0);
    orxVector_Set(&(sstDisplay.pstScreen->stClip.vBR), sstDisplay.pstScreen->fWidth, sstDisplay.pstScreen->fHeight, orxFLOAT_0);

    /* Creates texture for screen backup */
    glGenTextures(1, &(sstDisplay.pstScreen->uiTexture));
    glASSERT();
    glBindTexture(GL_TEXTURE_2D, sstDisplay.pstScreen->uiTexture);
    glASSERT();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sstDisplay.pstScreen->u32RealWidth, sstDisplay.pstScreen->u32RealHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (sstDisplay.pstScreen->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
    glASSERT();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (sstDisplay.pstScreen->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
    glASSERT();

    /* Clears destination bitmap */
    sstDisplay.pstDestinationBitmap = orxNULL;

    /* Clears new display surface */
    glScissor(0, 0, sstDisplay.pstScreen->u32RealWidth, sstDisplay.pstScreen->u32RealHeight);
    glASSERT();
    glClear(GL_COLOR_BUFFER_BIT);
    glASSERT();

    /* Enforces VSync status */
    orxDisplay_GLFW_EnableVSync(orxDisplay_GLFW_IsVSyncEnabled());

    /* Had bitmaps? */
    if(sstDisplay.s32BitmapCounter > 0)
    {
      orxBITMAP  *pstBitmap;
      orxU32      u32Index;

      /* For all bitmaps */
      for(pstBitmap = (orxBITMAP *)orxBank_GetNext(sstDisplay.pstBitmapBank, orxNULL), u32Index = 0;
          pstBitmap != orxNULL;
          pstBitmap = (orxBITMAP *)orxBank_GetNext(sstDisplay.pstBitmapBank, pstBitmap))
      {
        /* Not screen? */
        if(pstBitmap != sstDisplay.pstScreen)
        {
          /* Creates new texture */
          glGenTextures(1, &pstBitmap->uiTexture);
          glASSERT();
          glBindTexture(GL_TEXTURE_2D, pstBitmap->uiTexture);
          glASSERT();
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pstBitmap->u32RealWidth, pstBitmap->u32RealHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, sstDisplay.aau8BufferArray[u32Index]);
          glASSERT();
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glASSERT();
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          glASSERT();
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (pstBitmap->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
          glASSERT();
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (pstBitmap->bSmoothing != orxFALSE) ? GL_LINEAR : GL_NEAREST);
          glASSERT();

          /* Deletes buffer */
          orxMemory_Free(sstDisplay.aau8BufferArray[u32Index++]);
        }
      }

      /* Deletes buffer array */
      orxMemory_Free(sstDisplay.aau8BufferArray);
    }

    /* Had shaders? */
    if(sstDisplay.s32ShaderCounter > 0)
    {
      orxDISPLAY_SHADER *pstShader;

      /* For all shaders */
      for(pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, orxNULL);
          pstShader != orxNULL;
          pstShader = (orxDISPLAY_SHADER *)orxBank_GetNext(sstDisplay.pstShaderBank, pstShader))
      {
        /* Compiles it */
        orxDisplay_GLFW_CompileShader(pstShader);
      }
    }

    /* Clears counters */
    sstDisplay.s32BitmapCounter = sstDisplay.s32ShaderCounter = 0;
  }

  /* Clears last blend mode & last bitmap */
  sstDisplay.eLastBlendMode = orxDISPLAY_BLEND_MODE_NUMBER;
  sstDisplay.pstLastBitmap  = orxNULL;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetFullScreen(orxBOOL _bFullScreen)
{
  orxBOOL   bUpdate = orxFALSE;
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Enable? */
  if(_bFullScreen != orxFALSE)
  {
    /* Wasn't already full screen? */
    if(!orxFLAG_TEST_ALL(sstDisplay.u32GLFWFlags, GLFW_FULLSCREEN))
    {
      /* Updates window style */
      orxFLAG_SET(sstDisplay.u32GLFWFlags, GLFW_FULLSCREEN, GLFW_WINDOW);

      /* Asks for update */
      bUpdate = orxTRUE;
    }
  }
  else
  {
    /* Was full screen? */
    if(orxFLAG_TEST_ALL(sstDisplay.u32GLFWFlags, GLFW_FULLSCREEN))
    {
      /* Updates window style */
      orxFLAG_SET(sstDisplay.u32GLFWFlags, GLFW_WINDOW, GLFW_FULLSCREEN);

      /* Asks for update */
      bUpdate = orxTRUE;
    }
  }

  /* Should update? */
  if(bUpdate != orxFALSE)
  {
    orxDISPLAY_VIDEO_MODE stVideoMode;

    /* Inits video mode */
    stVideoMode.u32Width  = orxF2U(sstDisplay.pstScreen->fWidth);
    stVideoMode.u32Height = orxF2U(sstDisplay.pstScreen->fHeight);
    stVideoMode.u32Depth  = sstDisplay.pstScreen->u32Depth;

    /* Updates video mode */
    eResult = orxDisplay_GLFW_SetVideoMode(&stVideoMode);

    /* Failed? */
    if(eResult == orxSTATUS_FAILURE)
    {
      /* Restores previous full screen status */
      orxFLAG_SWAP(sstDisplay.u32GLFWFlags, GLFW_FULLSCREEN);

      /* Updates video mode */
      orxDisplay_GLFW_SetVideoMode(&stVideoMode);
    }
  }

  /* Done! */
  return eResult;
}

orxBOOL orxFASTCALL orxDisplay_GLFW_IsFullScreen()
{
  orxBOOL bResult = orxFALSE;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);

  /* Updates result */
  bResult = orxFLAG_TEST_ALL(sstDisplay.u32GLFWFlags, GLFW_FULLSCREEN) ? orxTRUE : orxFALSE;

  /* Done! */
  return bResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_Init()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Was not already initialized? */
  if(!(sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY))
  {
    /* Cleans static controller */
    orxMemory_Zero(&sstDisplay, sizeof(orxDISPLAY_STATIC));

    /* Inits GLFW */
    eResult = (glfwInit() != GL_FALSE) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

    /* Valid? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Creates banks */
      sstDisplay.pstBitmapBank  = orxBank_Create(orxDISPLAY_KU32_BITMAP_BANK_SIZE, sizeof(orxBITMAP), orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
      sstDisplay.pstShaderBank  = orxBank_Create(orxDISPLAY_KU32_SHADER_BANK_SIZE, sizeof(orxDISPLAY_SHADER), orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);

      /* Valid? */
      if((sstDisplay.pstBitmapBank != orxNULL)
      && (sstDisplay.pstShaderBank != orxNULL))
      {
        orxDISPLAY_VIDEO_MODE stVideoMode;

        /* Pushes display section */
        orxConfig_PushSection(orxDISPLAY_KZ_CONFIG_SECTION);

        /* Gets resolution from config */
        stVideoMode.u32Width  = orxConfig_HasValue(orxDISPLAY_KZ_CONFIG_WIDTH) ? orxConfig_GetU32(orxDISPLAY_KZ_CONFIG_WIDTH) : orxDISPLAY_KU32_SCREEN_WIDTH;
        stVideoMode.u32Height = orxConfig_HasValue(orxDISPLAY_KZ_CONFIG_HEIGHT) ? orxConfig_GetU32(orxDISPLAY_KZ_CONFIG_HEIGHT) : orxDISPLAY_KU32_SCREEN_HEIGHT;
        stVideoMode.u32Depth  = orxConfig_HasValue(orxDISPLAY_KZ_CONFIG_DEPTH) ? orxConfig_GetU32(orxDISPLAY_KZ_CONFIG_DEPTH) : orxDISPLAY_KU32_SCREEN_DEPTH;

        /* Full screen? */
        if(orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_FULLSCREEN) != orxFALSE)
        {
          /* Updates flags */
          sstDisplay.u32GLFWFlags = GLFW_FULLSCREEN;
        }
        else
        {
          /* Updates flags */
          sstDisplay.u32GLFWFlags = GLFW_WINDOW;
        }

        /* No decoration? */
        if((orxConfig_HasValue(orxDISPLAY_KZ_CONFIG_DECORATION) != orxFALSE)
        && (orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_DECORATION) == orxFALSE))
        {
          /* Logs message */
          orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "This GLFW plugin can't create windows with no decorations.");
        }

        /* Sets module as ready */
        sstDisplay.u32Flags = orxDISPLAY_KU32_STATIC_FLAG_READY;

        /* Allocates screen bitmap */
        sstDisplay.pstScreen = (orxBITMAP *)orxBank_Allocate(sstDisplay.pstBitmapBank);
        orxMemory_Zero(sstDisplay.pstScreen, sizeof(orxBITMAP));

        /* Sets video mode? */
        if((eResult = orxDisplay_GLFW_SetVideoMode(&stVideoMode)) == orxSTATUS_FAILURE)
        {
          /* Updates display flags */
          sstDisplay.u32GLFWFlags = GLFW_WINDOW | GLFW_WINDOW_NO_RESIZE;

          /* Updates resolution */
          stVideoMode.u32Width  = orxDISPLAY_KU32_SCREEN_WIDTH;
          stVideoMode.u32Height = orxDISPLAY_KU32_SCREEN_HEIGHT;
          stVideoMode.u32Depth  = orxDISPLAY_KU32_SCREEN_DEPTH;

          /* Sets video mode using default parameters */
          eResult = orxDisplay_GLFW_SetVideoMode(&stVideoMode);
        }

        /* Valid? */
        if(eResult != orxSTATUS_FAILURE)
        {
          orxCLOCK *pstClock;

          /* Updates its title */
          glfwSetWindowTitle(orxConfig_GetString(orxDISPLAY_KZ_CONFIG_TITLE));

          /* Has VSync value? */
          if(orxConfig_HasValue(orxDISPLAY_KZ_CONFIG_VSYNC) != orxFALSE)
          {
            /* Updates vertical sync */
            orxDisplay_GLFW_EnableVSync(orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_VSYNC));
          }
          else
          {
            /* Enables vertical sync */
            orxDisplay_GLFW_EnableVSync(orxTRUE);
          }

          /* Inits info */
          sstDisplay.bDefaultSmoothing  = orxConfig_GetBool(orxDISPLAY_KZ_CONFIG_SMOOTH);
          sstDisplay.eLastBlendMode     = orxDISPLAY_BLEND_MODE_NUMBER;

          /* Gets clock */
          pstClock = orxClock_FindFirst(orx2F(-1.0f), orxCLOCK_TYPE_CORE);

          /* Valid? */
          if(pstClock != orxNULL)
          {
            /* Registers update function */
            eResult = orxClock_Register(pstClock, orxDisplay_GLFW_Update, orxNULL, orxMODULE_ID_DISPLAY, orxCLOCK_PRIORITY_HIGHEST);
          }

          /* Shows mouse cursor */
          glfwEnable(GLFW_MOUSE_CURSOR);
        }
        else
        {
          /* Frees screen bitmap */
          orxBank_Free(sstDisplay.pstBitmapBank, sstDisplay.pstScreen);

          /* Deletes banks */
          orxBank_Delete(sstDisplay.pstBitmapBank);
          sstDisplay.pstBitmapBank = orxNULL;
          orxBank_Delete(sstDisplay.pstShaderBank);
          sstDisplay.pstShaderBank = orxNULL;

          /* Updates status */
          orxFLAG_SET(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_NONE, orxDISPLAY_KU32_STATIC_FLAG_READY);

          /* Logs message */
          orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to init GLFW/OpenGL default video mode.");
        }

        /* Pops config section */
        orxConfig_PopSection();
      }
      else
      {
        /* Deletes banks */
        if(sstDisplay.pstBitmapBank != orxNULL)
        {
          orxBank_Delete(sstDisplay.pstBitmapBank);
          sstDisplay.pstBitmapBank = orxNULL;
        }
        if(sstDisplay.pstShaderBank != orxNULL)
        {
          orxBank_Delete(sstDisplay.pstShaderBank);
          sstDisplay.pstShaderBank = orxNULL;
        }

        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to create bitmap/shader banks.");
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to init GLFW.");
    }
  }

  /* Done! */
  return eResult;
}

void orxFASTCALL orxDisplay_GLFW_Exit()
{
  /* Was initialized? */
  if(sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY)
  {
    /* Exits from GLFW */
    glfwTerminate();

    /* Unregisters update function */
    orxClock_Unregister(orxClock_FindFirst(orx2F(-1.0f), orxCLOCK_TYPE_CORE), orxDisplay_GLFW_Update);

    /* Deletes banks */
    orxBank_Delete(sstDisplay.pstBitmapBank);
    orxBank_Delete(sstDisplay.pstShaderBank);

    /* Cleans static controller */
    orxMemory_Zero(&sstDisplay, sizeof(orxDISPLAY_STATIC));
  }

  return;
}

orxHANDLE orxFASTCALL orxDisplay_GLFW_CreateShader(const orxSTRING _zCode, const orxLINKLIST *_pstParamList)
{
  orxHANDLE hResult = orxHANDLE_UNDEFINED;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstParamList != orxNULL);

  /* Has shader support? */
  if(orxFLAG_TEST(sstDisplay.u32Flags, orxDISPLAY_KU32_STATIC_FLAG_SHADER))
  {
    /* Valid? */
    if((_zCode != orxNULL) && (_zCode != orxSTRING_EMPTY))
    {
      orxDISPLAY_SHADER *pstShader;

      /* Creates a new shader */
      pstShader = (orxDISPLAY_SHADER *)orxBank_Allocate(sstDisplay.pstShaderBank);

      /* Successful? */
      if(pstShader != orxNULL)
      {
        orxSHADER_PARAM  *pstParam;
        orxCHAR          *pc;
        orxS32            s32Free;

        /* Inits shader code buffer */
        sstDisplay.acShaderCodeBuffer[0]  = sstDisplay.acShaderCodeBuffer[orxDISPLAY_KU32_SHADER_BUFFER_SIZE - 1] = orxCHAR_NULL;
        pc                                = sstDisplay.acShaderCodeBuffer;
        s32Free                           = orxDISPLAY_KU32_SHADER_BUFFER_SIZE - 1;

        /* For all parameters */
        for(pstParam = (orxSHADER_PARAM *)orxLinkList_GetFirst(_pstParamList);
            pstParam != orxNULL;
            pstParam = (orxSHADER_PARAM *)orxLinkList_GetNext(&(pstParam->stNode)))
        {
          /* Depending on type */
          switch(pstParam->eType)
          {
            case orxSHADER_PARAM_TYPE_FLOAT:
            {
              orxS32 s32Offset;

              /* Adds its literal value */
              s32Offset = orxString_NPrint(pc, s32Free, "uniform float %s;\n", pstParam->zName);
              pc       += s32Offset;
              s32Free  -= s32Offset;

              break;
            }

            case orxSHADER_PARAM_TYPE_TEXTURE:
            {
              orxS32 s32Offset;

              /* Adds its literal value and automated coordinates */
              s32Offset = orxString_NPrint(pc, s32Free, "uniform sampler2D %s;\nuniform float %s"orxDISPLAY_KZ_SHADER_SUFFIX_TOP";\nuniform float %s"orxDISPLAY_KZ_SHADER_SUFFIX_LEFT";\nuniform float %s"orxDISPLAY_KZ_SHADER_SUFFIX_BOTTOM";\nuniform float %s"orxDISPLAY_KZ_SHADER_SUFFIX_RIGHT";\n", pstParam->zName, pstParam->zName, pstParam->zName, pstParam->zName, pstParam->zName);
              pc       += s32Offset;
              s32Free  -= s32Offset;

              break;
            }

            case orxSHADER_PARAM_TYPE_VECTOR:
            {
              orxS32 s32Offset;

              /* Adds its literal value */
              s32Offset = orxString_NPrint(pc, s32Free, "uniform vec3 %s;\n", pstParam->zName);
              pc       += s32Offset;
              s32Free  -= s32Offset;

              break;
            }

            default:
            {
              break;
            }
          }
        }

        /* Adds code */
        orxString_NPrint(pc, s32Free, "%s\n", _zCode);

        /* Inits shader */
        pstShader->hProgram               = (GLhandleARB)orxHANDLE_UNDEFINED;
        pstShader->iTextureCounter        = 0;
        pstShader->bActive                = orxFALSE;
        pstShader->bInitialized           = orxFALSE;
        pstShader->zCode                  = orxString_Duplicate(sstDisplay.acShaderCodeBuffer);
        pstShader->astTextureInfoList     = (orxDISPLAY_TEXTURE_INFO *)orxMemory_Allocate(sstDisplay.iTextureUnitNumber * sizeof(orxDISPLAY_TEXTURE_INFO), orxMEMORY_TYPE_MAIN);
        orxMemory_Zero(pstShader->astTextureInfoList, sstDisplay.iTextureUnitNumber * sizeof(orxDISPLAY_TEXTURE_INFO));

        /* Compiles it */
        if(orxDisplay_GLFW_CompileShader(pstShader) != orxSTATUS_FAILURE)
        {
          /* Updates result */
          hResult = (orxHANDLE)pstShader;
        }
        else
        {
          /* Deletes code */
          orxString_Delete(pstShader->zCode);

          /* Deletes texture info list */
          orxMemory_Free(pstShader->astTextureInfoList);

          /* Frees shader */
          orxBank_Free(sstDisplay.pstShaderBank, pstShader);
        }
      }
    }
  }

  /* Done! */
  return hResult;
}

void orxFASTCALL orxDisplay_GLFW_DeleteShader(orxHANDLE _hShader)
{
  orxDISPLAY_SHADER *pstShader;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Deletes its program */
  glDeleteObjectARB(pstShader->hProgram);
  glASSERT();

  /* Deletes its code */
  orxString_Delete(pstShader->zCode);

  /* Deletes its texture info list */
  orxMemory_Free(pstShader->astTextureInfoList);

  /* Frees it */
  orxBank_Free(sstDisplay.pstShaderBank, pstShader);

  return;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_StartShader(orxHANDLE _hShader)
{
  orxDISPLAY_SHADER  *pstShader;
  orxSTATUS           eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Uses program */
  glUseProgramObjectARB(pstShader->hProgram);
  glASSERT();

  /* Updates its status */
  pstShader->bActive      = orxTRUE;
  pstShader->bInitialized = orxFALSE;

  /* Updates active shader counter */
  sstDisplay.s32ActiveShaderCounter++;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_StopShader(orxHANDLE _hShader)
{
  orxDISPLAY_SHADER  *pstShader;
  orxSTATUS           eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Wasn't initialized? */
  if(pstShader->bInitialized == orxFALSE)
  {
    GLfloat afVertexList[8];
    GLfloat afTextureCoordList[8];

    /* Inits it */
    orxDisplay_GLFW_InitShader(pstShader);

    /* Defines the vertex list */
    afVertexList[0] = sstDisplay.pstScreen->stClip.vTL.fX;
    afVertexList[1] = sstDisplay.pstScreen->stClip.vBR.fY;
    afVertexList[2] = sstDisplay.pstScreen->stClip.vTL.fX;
    afVertexList[3] = sstDisplay.pstScreen->stClip.vTL.fY;
    afVertexList[4] = sstDisplay.pstScreen->stClip.vBR.fX;
    afVertexList[5] = sstDisplay.pstScreen->stClip.vBR.fY;
    afVertexList[6] = sstDisplay.pstScreen->stClip.vBR.fX;
    afVertexList[7] = sstDisplay.pstScreen->stClip.vTL.fY;

    /* Defines the texture coord list */
    afTextureCoordList[0] = (GLfloat)(sstDisplay.pstScreen->fRecRealWidth * (sstDisplay.pstScreen->stClip.vTL.fX + orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[1] = (GLfloat)(orxFLOAT_1 - sstDisplay.pstScreen->fRecRealHeight * (sstDisplay.pstScreen->stClip.vBR.fY - orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[2] = (GLfloat)(sstDisplay.pstScreen->fRecRealWidth * (sstDisplay.pstScreen->stClip.vTL.fX + orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[3] = (GLfloat)(orxFLOAT_1 - sstDisplay.pstScreen->fRecRealHeight * (sstDisplay.pstScreen->stClip.vTL.fY + orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[4] = (GLfloat)(sstDisplay.pstScreen->fRecRealWidth * (sstDisplay.pstScreen->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[5] = (GLfloat)(orxFLOAT_1 - sstDisplay.pstScreen->fRecRealHeight * (sstDisplay.pstScreen->stClip.vBR.fY - orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[6] = (GLfloat)(sstDisplay.pstScreen->fRecRealWidth * (sstDisplay.pstScreen->stClip.vBR.fX - orxDISPLAY_KF_BORDER_FIX));
    afTextureCoordList[7] = (GLfloat)(orxFLOAT_1 - sstDisplay.pstScreen->fRecRealHeight * (sstDisplay.pstScreen->stClip.vTL.fY + orxDISPLAY_KF_BORDER_FIX));

    /* Selects arrays */
    glVertexPointer(2, GL_FLOAT, 0, afVertexList);
    glASSERT();
    glTexCoordPointer(2, GL_FLOAT, 0, afTextureCoordList);
    glASSERT();

    /* Draws arrays */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glASSERT();
  }

  /* Uses default program */
  glUseProgramObjectARB(0);
  glASSERT();

  /* Selects first texture unit */
  glActiveTextureARB(GL_TEXTURE0_ARB);
  glASSERT();

  /* Clears texture counter */
  pstShader->iTextureCounter = 0;

  /* Clears texture info list */
  orxMemory_Zero(pstShader->astTextureInfoList, sstDisplay.iTextureUnitNumber * sizeof(orxDISPLAY_TEXTURE_INFO));

  /* Updates its status */
  pstShader->bActive = orxFALSE;

  /* Updates active shader counter */
  sstDisplay.s32ActiveShaderCounter--;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetShaderBitmap(orxHANDLE _hShader, const orxSTRING _zParam, const orxBITMAP *_pstValue)
{
  orxDISPLAY_SHADER  *pstShader;
  orxCHAR             acBuffer[256];
  orxSTATUS           eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));
  orxASSERT(_zParam != orxNULL);

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Has free texture unit left? */
  if(pstShader->iTextureCounter < sstDisplay.iTextureUnitNumber)
  {
    GLint iLocation;

    /* Gets parameter location */
    iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)_zParam);
    glASSERT();

    /* Valid? */
    if(iLocation != -1)
    {
      /* No bitmap? */
      if(_pstValue == orxNULL)
      {
        /* Uses screen bitmap */
        _pstValue = sstDisplay.pstScreen;
      }

      /* Updates texture info */
      pstShader->astTextureInfoList[pstShader->iTextureCounter].iLocation = iLocation;
      pstShader->astTextureInfoList[pstShader->iTextureCounter].pstBitmap = _pstValue;

      /* Updates texture counter */
      pstShader->iTextureCounter++;

      /* Gets top parameter location */
      orxString_NPrint(acBuffer, 255, "%s"orxDISPLAY_KZ_SHADER_SUFFIX_TOP, _zParam);
      iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)acBuffer);
      glASSERT();

      /* Valid? */
      if(iLocation != -1)
      {
          /* Updates its value */
          glUniform1fARB(iLocation, (GLfloat)(orxFLOAT_1 - (_pstValue->fRecRealHeight * _pstValue->stClip.vTL.fY)));
          glASSERT();
      }

      /* Gets left parameter location */
      orxString_NPrint(acBuffer, 255, "%s"orxDISPLAY_KZ_SHADER_SUFFIX_LEFT, _zParam);
      iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)acBuffer);
      glASSERT();

      /* Valid? */
      if(iLocation != -1)
      {
          /* Updates its value */
          glUniform1fARB(iLocation, (GLfloat)(_pstValue->fRecRealWidth * _pstValue->stClip.vTL.fX));
          glASSERT();
      }

      /* Gets bottom parameter location */
      orxString_NPrint(acBuffer, 255, "%s"orxDISPLAY_KZ_SHADER_SUFFIX_BOTTOM, _zParam);
      iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)acBuffer);
      glASSERT();

      /* Valid? */
      if(iLocation != -1)
      {
          /* Updates its value */
          glUniform1fARB(iLocation, (GLfloat)(orxFLOAT_1 - (_pstValue->fRecRealHeight * _pstValue->stClip.vBR.fY)));
          glASSERT();
      }

      /* Gets right parameter location */
      orxString_NPrint(acBuffer, 255, "%s"orxDISPLAY_KZ_SHADER_SUFFIX_RIGHT, _zParam);
      iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)acBuffer);
      glASSERT();

      /* Valid? */
      if(iLocation != -1)
      {
          /* Updates its value */
          glUniform1fARB(iLocation, (GLfloat)(_pstValue->fRecRealWidth * _pstValue->stClip.vBR.fX));
          glASSERT();
      }

      /* Updates result */
      eResult = orxSTATUS_SUCCESS;
    }
    else
    {
      /* Outputs log */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Can't find texture parameter <%s> for fragment shader.", _zParam);

      /* Updates result */
      eResult = orxSTATUS_FAILURE;
    }
  }
  else
  {
    /* Outputs log */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Can't bind texture parameter <%s> for fragment shader: all the texture units are used.", _zParam);

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetShaderFloat(orxHANDLE _hShader, const orxSTRING _zParam, orxFLOAT _fValue)
{
  orxDISPLAY_SHADER  *pstShader;
  GLint               iLocation;
  orxSTATUS           eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));
  orxASSERT(_zParam != orxNULL);

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Gets parameter location */
  iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)_zParam);
  glASSERT();

  /* Valid? */
  if(iLocation != -1)
  {
    /* Updates its value */
    glUniform1fARB(iLocation, (GLfloat)_fValue);
    glASSERT();

    /* Updates result */
    eResult = orxSTATUS_SUCCESS;
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxDisplay_GLFW_SetShaderVector(orxHANDLE _hShader, const orxSTRING _zParam, const orxVECTOR *_pvValue)
{
  orxDISPLAY_SHADER  *pstShader;
  GLint               iLocation;
  orxSTATUS           eResult;

  /* Checks */
  orxASSERT((sstDisplay.u32Flags & orxDISPLAY_KU32_STATIC_FLAG_READY) == orxDISPLAY_KU32_STATIC_FLAG_READY);
  orxASSERT((_hShader != orxHANDLE_UNDEFINED) && (_hShader != orxNULL));
  orxASSERT(_zParam != orxNULL);
  orxASSERT(_pvValue != orxNULL);

  /* Gets shader */
  pstShader = (orxDISPLAY_SHADER *)_hShader;

  /* Gets parameter location */
  iLocation = glGetUniformLocationARB(pstShader->hProgram, (const GLcharARB *)_zParam);
  glASSERT();

  /* Valid? */
  if(iLocation != -1)
  {
    /* Updates its value */
    glUniform3fARB(iLocation, (GLfloat)_pvValue->fX, (GLfloat)_pvValue->fY, (GLfloat)_pvValue->fZ);
    glASSERT();

    /* Updates result */
    eResult = orxSTATUS_SUCCESS;
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}


/***************************************************************************
 * Plugin Related                                                          *
 ***************************************************************************/

orxPLUGIN_USER_CORE_FUNCTION_START(DISPLAY);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_Init, DISPLAY, INIT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_Exit, DISPLAY, EXIT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_Swap, DISPLAY, SWAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_CreateBitmap, DISPLAY, CREATE_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_DeleteBitmap, DISPLAY, DELETE_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SaveBitmap, DISPLAY, SAVE_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetDestinationBitmap, DISPLAY, SET_DESTINATION_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_TransformBitmap, DISPLAY, TRANSFORM_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_TransformText, DISPLAY, TRANSFORM_TEXT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_LoadBitmap, DISPLAY, LOAD_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetBitmapSize, DISPLAY, GET_BITMAP_SIZE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetScreenSize, DISPLAY, GET_SCREEN_SIZE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetScreen, DISPLAY, GET_SCREEN_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_ClearBitmap, DISPLAY, CLEAR_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetBitmapClipping, DISPLAY, SET_BITMAP_CLIPPING);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_BlitBitmap, DISPLAY, BLIT_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetBitmapData, DISPLAY, SET_BITMAP_DATA);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetBitmapColorKey, DISPLAY, SET_BITMAP_COLOR_KEY);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetBitmapColor, DISPLAY, SET_BITMAP_COLOR);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetBitmapColor, DISPLAY, GET_BITMAP_COLOR);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_CreateShader, DISPLAY, CREATE_SHADER);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_DeleteShader, DISPLAY, DELETE_SHADER);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_StartShader, DISPLAY, START_SHADER);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_StopShader, DISPLAY, STOP_SHADER);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetShaderBitmap, DISPLAY, SET_SHADER_BITMAP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetShaderFloat, DISPLAY, SET_SHADER_FLOAT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetShaderVector, DISPLAY, SET_SHADER_VECTOR);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_EnableVSync, DISPLAY, ENABLE_VSYNC);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_IsVSyncEnabled, DISPLAY, IS_VSYNC_ENABLED);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetFullScreen, DISPLAY, SET_FULL_SCREEN);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_IsFullScreen, DISPLAY, IS_FULL_SCREEN);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetVideoModeCounter, DISPLAY, GET_VIDEO_MODE_COUNTER);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_GetVideoMode, DISPLAY, GET_VIDEO_MODE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_SetVideoMode, DISPLAY, SET_VIDEO_MODE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxDisplay_GLFW_IsVideoModeAvailable, DISPLAY, IS_VIDEO_MODE_AVAILABLE);
orxPLUGIN_USER_CORE_FUNCTION_END();
