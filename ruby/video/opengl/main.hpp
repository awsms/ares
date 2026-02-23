#if defined(PLATFORM_MACOS)
#import <mach-o/dyld.h>
#endif
auto OpenGL::setShader(const string& pathname) -> void {
  settings.reset();

  format = inputFormat;
  filter = GL_NEAREST;
  wrap = GL_CLAMP_TO_BORDER;
  absoluteWidth = 0, absoluteHeight = 0;

  if(_chain != NULL) {
    _libra.gl_filter_chain_free(&_chain);
  }

  if(_preset != NULL) {
    _libra.preset_free(&_preset);
  }

  if(file::exists(pathname)) {
    if(_libra.preset_create(pathname.data(), &_preset) != NULL) {
      print(string{"OpenGL: Failed to load shader: ", pathname, "\n"});
      setShader("");
      return;
    }

    if(auto error = _libra.gl_filter_chain_create(&_preset, resolveSymbol, NULL, &_chain)) {
      print(string{"OpenGL: Failed to create filter chain for: ", pathname, "\n"});
      _libra.error_print(error);
      setShader("");
      return;
    }
  }
}

auto OpenGL::clear() -> void {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
}

auto OpenGL::setOverlay(const u32* data, u32 width, u32 height, s32 x, s32 y) -> void {
  if(!data || width == 0 || height == 0) {
    clearOverlay();
    return;
  }

  overlay.pixels.resize((u64)width * height);
  memory::copy<u32>(overlay.pixels.data(), data, overlay.pixels.size());
  overlay.width = width;
  overlay.height = height;
  overlay.x = x;
  overlay.y = y;
  overlay.visible = true;
  overlay.dirty = true;
}

auto OpenGL::clearOverlay() -> void {
  overlay.visible = false;
  overlay.dirty = false;
  overlay.pixels.clear();
  overlay.width = 0;
  overlay.height = 0;
  overlay.x = 0;
  overlay.y = 0;
}

auto OpenGL::renderOverlay() -> void {
  if(!overlay.visible || !overlay.width || !overlay.height || overlay.pixels.empty()) return;
  if(!targetWidth || !targetHeight || !outputWidth || !outputHeight) return;

  // The main game image is presented with framebuffer blits, which do not rely
  // on the GL viewport. The overlay card is rendered as a textured quad, so it
  // must track the current drawable size explicitly or it will drift when the
  // window/fullscreen viewport changes.
  glViewport(0, 0, outputWidth, outputHeight);

  if(!overlay.program) {
    auto compileShader = [&](GLenum type, const char* source) -> GLuint {
      auto shader = glCreateShader(type);
      glShaderSource(shader, 1, &source, nullptr);
      glCompileShader(shader);
      GLint status = GL_FALSE;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
      if(status != GL_TRUE) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        print(string{"OpenGL: Failed to compile OSD overlay shader: ", log, "\n"});
        glDeleteShader(shader);
        return 0;
      }
      return shader;
    };

    static const char* vertexSource = R"(
      #version 150 core
      in vec2 position;
      in vec2 texCoord;
      out vec2 vTexCoord;
      uniform vec2 viewportSize;
      void main() {
        vec2 ndc = vec2(
          (position.x / viewportSize.x) * 2.0 - 1.0,
          1.0 - (position.y / viewportSize.y) * 2.0
        );
        gl_Position = vec4(ndc, 0.0, 1.0);
        vTexCoord = texCoord;
      }
    )";

    static const char* fragmentSource = R"(
      #version 150 core
      in vec2 vTexCoord;
      out vec4 fragColor;
      uniform sampler2D overlayTexture;
      void main() {
        fragColor = texture(overlayTexture, vTexCoord);
      }
    )";

    auto vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    auto fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if(!vertexShader || !fragmentShader) {
      if(vertexShader) glDeleteShader(vertexShader);
      if(fragmentShader) glDeleteShader(fragmentShader);
      return;
    }

    overlay.program = glCreateProgram();
    glAttachShader(overlay.program, vertexShader);
    glAttachShader(overlay.program, fragmentShader);
    glBindFragDataLocation(overlay.program, 0, "fragColor");
    glLinkProgram(overlay.program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint status = GL_FALSE;
    glGetProgramiv(overlay.program, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
      char log[1024] = {};
      glGetProgramInfoLog(overlay.program, sizeof(log), nullptr, log);
      print(string{"OpenGL: Failed to link OSD overlay program: ", log, "\n"});
      glDeleteProgram(overlay.program);
      overlay.program = 0;
      return;
    }

    glGenVertexArrays(1, &overlay.vao);
    glGenBuffers(1, &overlay.vbo);
    glGenTextures(1, &overlay.texture);
  }

  glBindTexture(GL_TEXTURE_2D, overlay.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if(overlay.dirty) {
    glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA8, overlay.width, overlay.height, 0,
      GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, overlay.pixels.data()
    );
    overlay.dirty = false;
  }

  // Overlay coordinates are provided in a top-left-local space relative to the
  // final presented game rectangle. Convert them to top-left window-space here
  // so aspect correction, fullscreen centering, and shader output size all use
  // the same placement basis as the main blit.
  s32 finalX = (s32)targetX + overlay.x;
  s32 gameTop = (s32)outputHeight - (s32)(targetY + targetHeight);
  s32 finalY = gameTop + overlay.y;

  GLfloat vertices[] = {
    (GLfloat)finalX, (GLfloat)finalY, 0.0f, 0.0f,
    (GLfloat)(finalX + (s32)overlay.width), (GLfloat)finalY, 1.0f, 0.0f,
    (GLfloat)finalX, (GLfloat)(finalY + (s32)overlay.height), 0.0f, 1.0f,
    (GLfloat)(finalX + (s32)overlay.width), (GLfloat)(finalY + (s32)overlay.height), 1.0f, 1.0f,
  };

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(overlay.program);
  glBindVertexArray(overlay.vao);
  glBindBuffer(GL_ARRAY_BUFFER, overlay.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

  auto positionLocation = glGetAttribLocation(overlay.program, "position");
  auto texCoordLocation = glGetAttribLocation(overlay.program, "texCoord");
  glEnableVertexAttribArray(positionLocation);
  glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const void*)0);
  glEnableVertexAttribArray(texCoordLocation);
  glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const void*)(sizeof(GLfloat) * 2));

  glUniform2f(glGetUniformLocation(overlay.program, "viewportSize"), (GLfloat)outputWidth, (GLfloat)outputHeight);
  glUniform1i(glGetUniformLocation(overlay.program, "overlayTexture"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, overlay.texture);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(positionLocation);
  glDisableVertexAttribArray(texCoordLocation);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);
}

auto OpenGL::lock(u32*& data, u32& pitch) -> bool {
  pitch = width * sizeof(u32);
  return data = buffer;
}

auto OpenGL::output() -> void {
  clear();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, getFormat(), getType(), buffer);
  glGenerateMipmap(GL_TEXTURE_2D);

  struct Source {
    GLuint texture;
    u32 width, height;
    GLuint filter, wrap;
  };
  std::vector<Source> sources;
  sources.push_back({texture, width, height, filter, wrap});

  u32 targetWidth = absoluteWidth ? absoluteWidth : outputWidth;
  u32 targetHeight = absoluteHeight ? absoluteHeight : outputHeight;

  u32 x = (outputWidth - targetWidth) / 2;
  u32 y = (outputHeight - targetHeight) / 2;
  targetX = outputX + x;
  targetY = outputY + y;
  this->targetWidth = targetWidth;
  this->targetHeight = targetHeight;

  if(_chain != NULL) {
    // Shader path: our intermediate framebuffer matches the target size (final composited game area size)
    if(!framebuffer || framebufferWidth != targetWidth || framebufferHeight != targetHeight) {
      if(framebuffer) {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
      }
      if(framebufferTexture) {
        glDeleteTextures(1, &framebufferTexture);
        framebufferTexture = 0;
      }

      framebufferWidth = targetWidth, framebufferHeight = targetHeight;
      glGenFramebuffers(1, &framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      glGenTextures(1, &framebufferTexture);
      glBindTexture(GL_TEXTURE_2D, framebufferTexture);
      framebufferFormat = GL_RGB;

      glTexImage2D(GL_TEXTURE_2D, 0, framebufferFormat, framebufferWidth, framebufferHeight, 0, framebufferFormat,
                   GL_UNSIGNED_BYTE, nullptr);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferTexture, 0);
    }
  } else {
    // Non-shader path: our intermediate framebuffer matches the source size and re-uses the source texture
    if(!framebuffer || framebufferWidth != width || framebufferHeight != height) {
      if(framebuffer) {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
      }

      if(framebufferTexture) {
        glDeleteTextures(1, &framebufferTexture);
        framebufferTexture = 0;
      }

      framebufferWidth = width, framebufferHeight = height;
      glGenFramebuffers(1, &framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    }
  }

  render(sources[0].width, sources[0].height, targetX, targetY, targetWidth, targetHeight);
  renderOverlay();
}

auto OpenGL::initialize(const string& shader) -> bool {
  if(!OpenGLBind()) return false;

  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_POLYGON_SMOOTH);
  glDisable(GL_STENCIL_TEST);
  glEnable(GL_DITHER);

  _libra = librashader_load_instance();
  if(!_libra.instance_loaded) {
    print("OpenGL: Failed to load librashader: shaders will be disabled\n");
  }

  setShader(shader);
  return initialized = true;
}

auto OpenGL::resolveSymbol(const char* name) -> const void * {
#if defined(PLATFORM_MACOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSSymbol symbol;
  char *symbolName;
  symbolName = (char*)malloc(strlen(name) + 2);
  strcpy(symbolName + 1, name);
  symbolName[0] = '_';
  symbol = NULL;
  if(NSIsSymbolNameDefined (symbolName)) symbol = NSLookupAndBindSymbol (symbolName);
  free(symbolName); // 5
  return (void*)(symbol ? NSAddressOfSymbol(symbol) : NULL);
#pragma clang diagnostic pop
#else
  void* symbol = (void*)glGetProcAddress(name);
  #if defined(PLATFORM_WINDOWS)
    if(!symbol) {
      // (w)glGetProcAddress will not return function pointers from any OpenGL functions
      // that are directly exported by the opengl32.dll
      HMODULE module = LoadLibraryA("opengl32.dll");
      symbol = (void*)GetProcAddress(module, name);
    }
  #endif
#endif

  return symbol;
}

auto OpenGL::terminate() -> void {
  if(!initialized) return;
  setShader("");
  if(overlay.texture) { glDeleteTextures(1, &overlay.texture); overlay.texture = 0; }
  if(overlay.vbo) { glDeleteBuffers(1, &overlay.vbo); overlay.vbo = 0; }
  if(overlay.vao) { glDeleteVertexArrays(1, &overlay.vao); overlay.vao = 0; }
  if(overlay.program) { glDeleteProgram(overlay.program); overlay.program = 0; }
  clearOverlay();
  targetX = 0;
  targetY = 0;
  targetWidth = 0;
  targetHeight = 0;
  OpenGLSurface::release();
  if(buffer) { delete[] buffer; buffer = nullptr; }
  initialized = false;
}
