// Independent nijilive OpenGL diff_collect_impl.d, BSD-2-Clause.
// Copyright (c) 2020 Inochi2D Project; (c) 2024 nijigenerate Project.
// Full notice: native/port/licenses/nijilive-difference-aggregation.txt.
#include "runtime_gl_diff_collect.hpp"
#ifndef NICXLIVE_SOURCE_NO_RENDER
#include "runtime_gl_diff_collect_assets.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <common/shared_array.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <memory>
#include <utility>
#define glActiveTexture ::nijigenerate::platform::ngGLBackendProcedure<PFNGLACTIVETEXTUREPROC>("glActiveTexture")
#define glAttachShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLATTACHSHADERPROC>("glAttachShader")
#define glBindFramebuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")
#define glBindTexture ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glBindTexture)>("glBindTexture")
#define glBindVertexArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray")
#define glCheckFramebufferStatus ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus")
#define glClear ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glClear)>("glClear")
#define glClearColor ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glClearColor)>("glClearColor")
#define glCompileShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCOMPILESHADERPROC>("glCompileShader")
#define glCreateProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCREATEPROGRAMPROC>("glCreateProgram")
#define glCreateShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCREATESHADERPROC>("glCreateShader")
#define glDeleteFramebuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers")
#define glDeleteProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETEPROGRAMPROC>("glDeleteProgram")
#define glDeleteShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETESHADERPROC>("glDeleteShader")
#define glDeleteTextures ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDeleteTextures)>("glDeleteTextures")
#define glDeleteVertexArrays ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays")
#define glDisable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDisable)>("glDisable")
#define glDrawArrays ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDrawArrays)>("glDrawArrays")
#define glDrawBuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDRAWBUFFERSPROC>("glDrawBuffers")
#define glEnable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glEnable)>("glEnable")
#define glFramebufferTexture2D ::nijigenerate::platform::ngGLBackendProcedure<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D")
#define glGenFramebuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers")
#define glGenTextures ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGenTextures)>("glGenTextures")
#define glGenVertexArrays ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays")
#define glGetIntegerv ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGetIntegerv)>("glGetIntegerv")
#define glGetProgramInfoLog ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog")
#define glGetProgramiv ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETPROGRAMIVPROC>("glGetProgramiv")
#define glGetShaderInfoLog ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog")
#define glGetShaderiv ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETSHADERIVPROC>("glGetShaderiv")
#define glGetTexImage ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGetTexImage)>("glGetTexImage")
#define glGetUniformLocation ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation")
#define glIsEnabled ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glIsEnabled)>("glIsEnabled")
#define glLinkProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLLINKPROGRAMPROC>("glLinkProgram")
#define glShaderSource ::nijigenerate::platform::ngGLBackendProcedure<PFNGLSHADERSOURCEPROC>("glShaderSource")
#define glTexImage2D ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glTexImage2D)>("glTexImage2D")
#define glTexParameteri ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glTexParameteri)>("glTexParameteri")
#define glUniform1i ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUNIFORM1IPROC>("glUniform1i")
#define glUniform2i ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUNIFORM2IPROC>("glUniform2i")
#define glUniform4i ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUNIFORM4IPROC>("glUniform4i")
#define glUseProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUSEPROGRAMPROC>("glUseProgram")
#define glViewport ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glViewport)>("glViewport")
namespace nijigenerate::backend::runtime_gl {
using nicxlive::core::DifferenceEvaluationRegion;
using nicxlive::core::DifferenceEvaluationResult;
using nicxlive::core::RenderResourceHandle;
namespace {
using std::clamp;
using std::min;
using std::max;
using std::floor;
constexpr int TileColumns = 16;
constexpr int TileCount = TileColumns * TileColumns;
constexpr std::array<GLenum, 1> colorAttachment{GL_COLOR_ATTACHMENT0};
DifferenceEvaluationResult initialResult() {
    DifferenceEvaluationResult value;
    value.red = value.green = value.blue = value.alpha = std::numeric_limits<double>::quiet_NaN();
    value.tileTotals.fill(std::numeric_limits<double>::quiet_NaN());
    value.tileCounts.fill(std::numeric_limits<double>::quiet_NaN());
    return value;
}
GLuint compileShader(GLenum stage, std::string_view source);
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
compat::RetainedString logMessage(std::string_view prefix, const compat::RetainedString& message) {
    const auto bytes = message.view();
    std::string result;
    result.reserve(prefix.size() + bytes.size());
    result.append(prefix);
    result.append(bytes);
    return compat::RetainedString(std::move(result));
}
using difference_assets::collectVertex;
using difference_assets::collectFragment;
using difference_assets::reduceFragment;
struct ReductionLevel {
    GLuint fbo{};
    GLuint texture{};
    int width{};
    int height{};
};

struct DifferenceEvaluator {
    bool enabled{};
    bool useRegion{};
    DifferenceEvaluationRegion region{};

    bool resultReady{};
    std::uint32_t pendingSampleCount{};

    GLuint collectProgram{};
    GLuint reduceProgram{};
    GLuint fullscreenVao{};

    GLuint collectFbo{};
    GLuint collectTexture{};

    nicxlive::core::common::SharedArray<ReductionLevel> reductionLevels;
    GLuint lastTexture{};

    int resourceWidth{};
    int resourceHeight{};
    int finalWidth{};
    int finalHeight{};

    GLint uCollectViewport{};
    GLint uCollectRect{};
    GLint uCollectUseRect{};
    GLint uCollectSource{};

    GLint uReduceSourceSize{};
    GLint uReduceSource{};

    void setEnabled(bool value) {
        enabled = value;
        if (!enabled) {
            resultReady = false;
            pendingSampleCount = 0;
        }
    }

    void setRegion(DifferenceEvaluationRegion region) {
        this->region = region;
        useRegion = region.width > 0 && region.height > 0;
    }

    DifferenceEvaluationRegion getRegion() const {
        return region;
    }

    bool isEnabled() const {
        return enabled;
    }

    void ensurePrograms() {
        if (collectProgram != 0 && reduceProgram != 0 && fullscreenVao != 0) {
            return;
        }

        GLuint vertShader = compileShader(GL_VERTEX_SHADER, collectVertex);
        GLuint fragCollect = compileShader(GL_FRAGMENT_SHADER, collectFragment);
        GLuint fragReduce = compileShader(GL_FRAGMENT_SHADER, reduceFragment);

        collectProgram = linkProgram(vertShader, fragCollect);
        reduceProgram = linkProgram(vertShader, fragReduce);

        glDeleteShader(vertShader);
        glDeleteShader(fragCollect);
        glDeleteShader(fragReduce);

        uCollectViewport = glGetUniformLocation(collectProgram, "uViewportSize");
        uCollectRect = glGetUniformLocation(collectProgram, "uRect");
        uCollectUseRect = glGetUniformLocation(collectProgram, "uUseRect");
        uCollectSource = glGetUniformLocation(collectProgram, "uSource");

        uReduceSourceSize = glGetUniformLocation(reduceProgram, "uSourceSize");
        uReduceSource = glGetUniformLocation(reduceProgram, "uSource");

        glGenVertexArrays(1, &fullscreenVao);
    }

    void ensureResources(int width, int height) {
        if (resourceWidth == width && resourceHeight == height) {
            return;
        }

        destroyResources();

        resourceWidth = width;
        resourceHeight = height;
        finalWidth = width;
        finalHeight = height;

        collectTexture = createTexture(width, height);
        collectFbo = createFramebuffer(collectTexture);

        int currentWidth = width;
        int currentHeight = height;

        while (currentWidth > 16 || currentHeight > 16) {
            currentWidth = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(currentWidth) + 1u) / 2;
            currentHeight = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(currentHeight) + 1u) / 2;

            ReductionLevel level;
            level.width = currentWidth;
            level.height = currentHeight;
            level.texture = createTexture(level.width, level.height);
            level.fbo = createFramebuffer(level.texture);
            reductionLevels.push_back(level);
        }
    }

    GLuint createTexture(int width, int height) {
        GLuint tex{};
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    GLuint createFramebuffer(GLuint texture) {
        GLuint fbo{};
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw compat::ThrowableException(compat::RetainedString(std::string(
                "Failed to create framebuffer for difference aggregation")),
                compat::RetainedString::fromStaticLiteral(
                    "nijilive/core/render/backends/opengl/diff_collect_impl.d"), 148);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return fbo;
    }

    void destroyResources() {
        auto capturedLevels = reductionLevels;
        for (auto& level : capturedLevels) {
            if (level.fbo) glDeleteFramebuffers(1, &level.fbo);
            if (level.texture) glDeleteTextures(1, &level.texture);
        }
        reductionLevels.resize(0);

        if (collectFbo) glDeleteFramebuffers(1, &collectFbo);
        if (collectTexture) glDeleteTextures(1, &collectTexture);

        collectFbo = 0;
        collectTexture = 0;
        lastTexture = 0;
        resourceWidth = resourceHeight = 0;
        finalWidth = finalHeight = 0;
    }

    void destroyPrograms() {
        if (collectProgram) glDeleteProgram(collectProgram);
        if (reduceProgram) glDeleteProgram(reduceProgram);
        if (fullscreenVao) glDeleteVertexArrays(1, &fullscreenVao);

        collectProgram = 0;
        reduceProgram = 0;
        fullscreenVao = 0;
        uCollectViewport = -1;
        uCollectRect = -1;
        uCollectUseRect = -1;
        uCollectSource = -1;
        uReduceSourceSize = -1;
        uReduceSource = -1;
    }

    bool evaluate(GLuint sourceTexture, int viewportWidth, int viewportHeight) {
        if (!enabled) {
            resultReady = false;
            pendingSampleCount = 0;
            return false;
        }
        if (sourceTexture == 0 || viewportWidth <= 0 || viewportHeight <= 0) {
            resultReady = false;
            pendingSampleCount = 0;
            return false;
        }

        ensurePrograms();

        DifferenceEvaluationRegion clampedRegion = region;
        if (useRegion) {
            clampedRegion.x = clamp(clampedRegion.x, 0, viewportWidth);
            clampedRegion.y = clamp(clampedRegion.y, 0, viewportHeight);
            clampedRegion.width = clamp(clampedRegion.width, 0, viewportWidth - clampedRegion.x);
            clampedRegion.height = clamp(clampedRegion.height, 0, viewportHeight - clampedRegion.y);
        }

        int targetWidth = useRegion ? clampedRegion.width : viewportWidth;
        int targetHeight = useRegion ? clampedRegion.height : viewportHeight;
        ensureResources(targetWidth, targetHeight);
        if (targetWidth <= 0 || targetHeight <= 0) {
            resultReady = false;
            pendingSampleCount = 0;
            return false;
        }

        pendingSampleCount = static_cast<std::uint32_t>(targetWidth) * static_cast<std::uint32_t>(targetHeight);

        GLint prevFbo{};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

        std::array<GLint, 4> prevViewport{};
        glGetIntegerv(GL_VIEWPORT, prevViewport.data());

        GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glBindFramebuffer(GL_FRAMEBUFFER, collectFbo);
        glDrawBuffers(1, colorAttachment.data());
        glViewport(0, 0, targetWidth, targetHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(collectProgram);
        if (uCollectViewport != -1) glUniform2i(uCollectViewport, viewportWidth, viewportHeight);
        if (uCollectRect != -1) glUniform4i(uCollectRect,
            clampedRegion.x,
            clampedRegion.y,
            targetWidth,
            targetHeight
        );
        if (uCollectUseRect != -1) glUniform1i(uCollectUseRect, useRegion ? 1 : 0);
        if (uCollectSource != -1) glUniform1i(uCollectSource, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);

        glBindVertexArray(fullscreenVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        GLuint currentTexture = collectTexture;
        int currentWidth = targetWidth;
        int currentHeight = targetHeight;

        auto capturedLevels = reductionLevels;
        for (auto& level : capturedLevels) {
            if (level.width < TileColumns || level.height < TileColumns) {
                break;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, level.fbo);
            glDrawBuffers(1, colorAttachment.data());
            glViewport(0, 0, level.width, level.height);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(reduceProgram);
            if (uReduceSourceSize != -1) glUniform2i(uReduceSourceSize, currentWidth, currentHeight);
            if (uReduceSource != -1) glUniform1i(uReduceSource, 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentTexture);

            glBindVertexArray(fullscreenVao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            currentTexture = level.texture;
            currentWidth = level.width;
            currentHeight = level.height;
        }

        lastTexture = currentTexture;
        finalWidth = currentWidth;
        finalHeight = currentHeight;
        resultReady = true;

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

        return true;
    }

    bool fetch(DifferenceEvaluationResult& result) {
        result = initialResult();
        if (!resultReady || lastTexture == 0) {
            result = initialResult();
            return false;
        }

        int width = finalWidth > 0 ? finalWidth : 1;
        int height = finalHeight > 0 ? finalHeight : 1;
        std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

        nicxlive::core::common::SharedArray<float> data(pixelCount * 4,
            std::numeric_limits<float>::quiet_NaN());
        glBindTexture(GL_TEXTURE_2D, lastTexture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        double totalRed = 0;
        double totalGreen = 0;
        double totalBlue = 0;
        double totalAlpha = 0;

        std::array<double, TileCount> tileTotals{};
        std::array<double, TileCount> tileCounts{};

        double regionWidth = resourceWidth > 0 ? static_cast<double>(resourceWidth) : static_cast<double>(width);
        double regionHeight = resourceHeight > 0 ? static_cast<double>(resourceHeight) : static_cast<double>(height);
        if (regionWidth <= 0 || regionHeight <= 0) {
            result = initialResult();
            return false;
        }

        double tileWidth = regionWidth / TileColumns;
        double tileHeight = regionHeight / TileColumns;
        if (tileWidth <= 0 || tileHeight <= 0) {
            result = initialResult();
            return false;
        }

        double pixelWidth = regionWidth / static_cast<double>(width);
        double pixelHeight = regionHeight / static_cast<double>(height);
        if (pixelWidth <= 0 || pixelHeight <= 0) {
            result = initialResult();
            return false;
        }
        double pixelArea = pixelWidth * pixelHeight;
        if (pixelArea <= 0) {
            result = initialResult();
            return false;
        }
        const double epsilon = 1e-9;

        for (int y = 0; y < height; ++y) {
            double srcTop = y * pixelHeight;
            double srcBottom = (y + 1) * pixelHeight;

            for (int x = 0; x < width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x) * 4;

                double r = data[idx + 0];
                double g = data[idx + 1];
                double b = data[idx + 2];
                double a = data[idx + 3];

                totalRed += r;
                totalGreen += g;
                totalBlue += b;
                double weight = a > 0 ? a : 1.0;
                totalAlpha += weight;

                double srcLeft = x * pixelWidth;
                double srcRight = (x + 1) * pixelWidth;

                int tileXStart = static_cast<int>(floor(srcLeft / tileWidth));
                int tileXEnd = static_cast<int>(floor((srcRight - epsilon) / tileWidth));
                int tileYStart = static_cast<int>(floor(srcTop / tileHeight));
                int tileYEnd = static_cast<int>(floor((srcBottom - epsilon) / tileHeight));

                tileXStart = clamp(tileXStart, 0, TileColumns - 1);
                tileXEnd = clamp(tileXEnd, 0, TileColumns - 1);
                tileYStart = clamp(tileYStart, 0, TileColumns - 1);
                tileYEnd = clamp(tileYEnd, 0, TileColumns - 1);

                double sampleTotal = r + g + b;
                for (int tileY = tileYStart; tileY < tileYEnd + 1; ++tileY) {
                    double tileTop = tileY * tileHeight;
                    double tileBottom = tileTop + tileHeight;
                    double overlapY = min(srcBottom, tileBottom) - max(srcTop, tileTop);
                    if (overlapY <= 0) continue;

                    for (int tileX = tileXStart; tileX < tileXEnd + 1; ++tileX) {
                        double tileLeft = tileX * tileWidth;
                        double tileRight = tileLeft + tileWidth;
                        double overlapX = min(srcRight, tileRight) - max(srcLeft, tileLeft);
                        if (overlapX <= 0) continue;

                        double overlapArea = overlapX * overlapY;
                        if (overlapArea <= 0) continue;

                        double fraction = overlapArea / pixelArea;
                        if (fraction <= 0) continue;

                        std::size_t tileIndex = static_cast<std::size_t>(tileY) * TileColumns + tileX;
                        tileTotals[tileIndex] += sampleTotal * fraction;
                        tileCounts[tileIndex] += weight * fraction;
                    }
                }
            }
        }

        if (totalAlpha <= 0) {
            totalAlpha = static_cast<double>(pendingSampleCount);
            for (auto& count : tileCounts) if (count == 0) count = 1.0;
        }

        result.red = totalRed;
        result.green = totalGreen;
        result.blue = totalBlue;
        result.alpha = totalAlpha;
        result.sampleCount = static_cast<std::uint32_t>(totalAlpha);
        result.tileTotals = tileTotals;
        result.tileCounts = tileCounts;

        resultReady = false;
        return true;
    }

    ~DifferenceEvaluator() noexcept(false) {
        destroyResources();
        destroyPrograms();
    }
};

thread_local DifferenceEvaluator gDifferenceEvaluator;

} // namespace
void oglSetDifferenceAggregationEnabled(bool enabled) {
    gDifferenceEvaluator.setEnabled(enabled);
}

bool oglIsDifferenceAggregationEnabled() {
    return gDifferenceEvaluator.isEnabled();
}

void oglSetDifferenceAggregationRegion(DifferenceEvaluationRegion region) {
    gDifferenceEvaluator.setRegion(region);
}

DifferenceEvaluationRegion oglGetDifferenceAggregationRegion() {
    return gDifferenceEvaluator.getRegion();
}

bool oglEvaluateDifferenceAggregation(RenderResourceHandle texture, int viewportWidth, int viewportHeight) {
    return gDifferenceEvaluator.evaluate(static_cast<GLuint>(texture), viewportWidth, viewportHeight);
}

bool oglFetchDifferenceAggregationResult(DifferenceEvaluationResult& result) {
    result = DifferenceEvaluationResult{};
    return gDifferenceEvaluator.fetch(result);
}

namespace {
GLuint compileShader(GLenum stage, std::string_view source) {
    GLuint shader = glCreateShader(stage);
    auto ptr = source.data();
    glShaderSource(shader, 1, &ptr, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    auto message = compat::RetainedString::fromStaticLiteral("unknown");
    if (length > 0) {
        auto buffer = std::make_shared<std::string>(static_cast<std::size_t>(length), '\xff');
        glGetShaderInfoLog(shader, length, nullptr, buffer->data());
        message = compat::RetainedString::retainedView(buffer, buffer->data(), buffer->size());
    }

    glDeleteShader(shader);
    throw compat::ThrowableException(logMessage("Failed to compile shader: ", message),
        compat::RetainedString::fromStaticLiteral(
            "nijilive/core/render/backends/opengl/diff_collect_impl.d"), 484);
}

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) {
        return program;
    }

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    auto message = compat::RetainedString::fromStaticLiteral("unknown");
    if (length > 0) {
        auto buffer = std::make_shared<std::string>(static_cast<std::size_t>(length), '\xff');
        glGetProgramInfoLog(program, length, nullptr, buffer->data());
        message = compat::RetainedString::retainedView(buffer, buffer->data(), buffer->size());
    }
    glDeleteProgram(program);
    throw compat::ThrowableException(logMessage("Failed to link shader program: ", message),
        compat::RetainedString::fromStaticLiteral(
            "nijilive/core/render/backends/opengl/diff_collect_impl.d"), 508);
}

} // namespace
} // namespace nijigenerate::backend::runtime_gl
#undef glActiveTexture
#undef glAttachShader
#undef glBindFramebuffer
#undef glBindTexture
#undef glBindVertexArray
#undef glCheckFramebufferStatus
#undef glClear
#undef glClearColor
#undef glCompileShader
#undef glCreateProgram
#undef glCreateShader
#undef glDeleteFramebuffers
#undef glDeleteProgram
#undef glDeleteShader
#undef glDeleteTextures
#undef glDeleteVertexArrays
#undef glDisable
#undef glDrawArrays
#undef glDrawBuffers
#undef glEnable
#undef glFramebufferTexture2D
#undef glGenFramebuffers
#undef glGenTextures
#undef glGenVertexArrays
#undef glGetIntegerv
#undef glGetProgramInfoLog
#undef glGetProgramiv
#undef glGetShaderInfoLog
#undef glGetShaderiv
#undef glGetTexImage
#undef glGetUniformLocation
#undef glIsEnabled
#undef glLinkProgram
#undef glShaderSource
#undef glTexImage2D
#undef glTexParameteri
#undef glUniform1i
#undef glUniform2i
#undef glUniform4i
#undef glUseProgram
#undef glViewport
#else
namespace nijigenerate::backend::runtime_gl {
void oglSetDifferenceAggregationEnabled(bool) {}
bool oglIsDifferenceAggregationEnabled() { return false; }
void oglSetDifferenceAggregationRegion(nicxlive::core::DifferenceEvaluationRegion) {}
nicxlive::core::DifferenceEvaluationRegion oglGetDifferenceAggregationRegion() { return {}; }
bool oglEvaluateDifferenceAggregation(nicxlive::core::RenderResourceHandle, int, int) { return false; }
bool oglFetchDifferenceAggregationResult(nicxlive::core::DifferenceEvaluationResult& result) {
    result = nicxlive::core::DifferenceEvaluationResult{};
    return false;
}
} // namespace nijigenerate::backend::runtime_gl
#endif
