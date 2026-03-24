#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Behavioral reference: EasyRPG Player's RPG_RT transition mapping and frame timing.
// This file provides an original SDL implementation of the same transition family.
class RpgRtTransition {
public:
    enum class Type {
        FadeIn,
        FadeOut,
        RandomBlocks,
        RandomBlocksDown,
        RandomBlocksUp,
        BlindOpen,
        BlindClose,
        VerticalStripesIn,
        VerticalStripesOut,
        HorizontalStripesIn,
        HorizontalStripesOut,
        BorderToCenterIn,
        BorderToCenterOut,
        CenterToBorderIn,
        CenterToBorderOut,
        ScrollUpIn,
        ScrollDownIn,
        ScrollLeftIn,
        ScrollRightIn,
        ScrollUpOut,
        ScrollDownOut,
        ScrollLeftOut,
        ScrollRightOut,
        VerticalCombine,
        VerticalDivision,
        HorizontalCombine,
        HorizontalDivision,
        CrossCombine,
        CrossDivision,
        ZoomIn,
        ZoomOut,
        MosaicIn,
        MosaicOut,
        WaveIn,
        WaveOut,
        CutIn,
        CutOut,
        None
    };

    struct Options {
        int duration = -1;
        SDL_Point zoomCenter = {-1, -1};
        uint32_t seed = 0;
    };

    RpgRtTransition() = default;

    ~RpgRtTransition() {
        reset();
    }

    RpgRtTransition(const RpgRtTransition&) = delete;
    RpgRtTransition& operator=(const RpgRtTransition&) = delete;

    static int DefaultFrames(Type type) {
        switch (type) {
            case Type::FadeIn:
            case Type::FadeOut:
                return 35;
            case Type::CutIn:
            case Type::CutOut:
                return 1;
            case Type::None:
                return 0;
            default:
                return 41;
        }
    }

    static Type FromShowTransitionId(int id) {
        switch (id) {
            case 0: return Type::FadeIn;
            case 1: return Type::RandomBlocks;
            case 2: return Type::RandomBlocksDown;
            case 3: return Type::RandomBlocksUp;
            case 4: return Type::BlindOpen;
            case 5: return Type::VerticalStripesIn;
            case 6: return Type::HorizontalStripesIn;
            case 7: return Type::BorderToCenterIn;
            case 8: return Type::CenterToBorderIn;
            case 9: return Type::ScrollUpIn;
            case 10: return Type::ScrollDownIn;
            case 11: return Type::ScrollLeftIn;
            case 12: return Type::ScrollRightIn;
            case 13: return Type::VerticalCombine;
            case 14: return Type::HorizontalCombine;
            case 15: return Type::CrossCombine;
            case 16: return Type::ZoomOut;
            case 17: return Type::MosaicIn;
            case 18: return Type::WaveIn;
            case 19: return Type::CutIn;
            default: return Type::None;
        }
    }

    static Type FromEraseTransitionId(int id) {
        switch (id) {
            case 0: return Type::FadeOut;
            case 1: return Type::RandomBlocks;
            case 2: return Type::RandomBlocksDown;
            case 3: return Type::RandomBlocksUp;
            case 4: return Type::BlindClose;
            case 5: return Type::VerticalStripesOut;
            case 6: return Type::HorizontalStripesOut;
            case 7: return Type::BorderToCenterOut;
            case 8: return Type::CenterToBorderOut;
            case 9: return Type::ScrollUpOut;
            case 10: return Type::ScrollDownOut;
            case 11: return Type::ScrollLeftOut;
            case 12: return Type::ScrollRightOut;
            case 13: return Type::VerticalDivision;
            case 14: return Type::HorizontalDivision;
            case 15: return Type::CrossDivision;
            case 16: return Type::ZoomIn;
            case 17: return Type::MosaicOut;
            case 18: return Type::WaveOut;
            case 19: return Type::CutOut;
            default: return Type::None;
        }
    }

    static SDL_Surface* CaptureRenderer(SDL_Renderer* renderer, int width, int height) {
        if (renderer == nullptr || width <= 0 || height <= 0) {
            return nullptr;
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface) {
            return nullptr;
        }

        if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
            SDL_FreeSurface(surface);
            return nullptr;
        }

        return surface;
    }

    bool begin(SDL_Renderer* renderer,
               Type type,
               const SDL_Surface* fromSurface,
               const SDL_Surface* toSurface,
               Options options = {}) {
        reset();

        m_type = type;
        m_totalFrames = options.duration >= 0 ? options.duration : DefaultFrames(type);
        m_currentFrame = (m_totalFrames > 0) ? -1 : 0;

        m_from = copyAsArgb32(fromSurface);
        m_to = copyAsArgb32(toSurface);
        if (!m_from && !m_to) {
            return false;
        }

        if (!m_from) {
            m_from = createBlackLike(m_to);
        }
        if (!m_to) {
            m_to = createBlackLike(m_from);
        }
        if (!m_from || !m_to) {
            reset();
            return false;
        }
        if (m_from->w != m_to->w || m_from->h != m_to->h) {
            reset();
            return false;
        }

        m_work = SDL_CreateRGBSurfaceWithFormat(0, m_from->w, m_from->h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!m_work) {
            reset();
            return false;
        }

        m_texture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      m_from->w,
                                      m_from->h);
        if (!m_texture) {
            reset();
            return false;
        }
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);

        m_zoomCenter = options.zoomCenter;
        if (m_zoomCenter.x < 0 || m_zoomCenter.x > m_from->w) {
            m_zoomCenter.x = m_from->w / 2;
        }
        if (m_zoomCenter.y < 0 || m_zoomCenter.y > m_from->h) {
            m_zoomCenter.y = m_from->h / 2;
        }

        prepareTransitionState(options.seed);
        composeCurrentFrame();
        return true;
    }

    void reset() {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }
        if (m_work) {
            SDL_FreeSurface(m_work);
            m_work = nullptr;
        }
        if (m_from) {
            SDL_FreeSurface(m_from);
            m_from = nullptr;
        }
        if (m_to) {
            SDL_FreeSurface(m_to);
            m_to = nullptr;
        }

        m_type = Type::None;
        m_totalFrames = 0;
        m_currentFrame = 0;
        m_randomBlocks.clear();
        m_mosaicRandomOffset.clear();
    }

    void advance(int frames = 1) {
        if (frames <= 0 || !m_work) {
            return;
        }

        m_currentFrame = std::min(m_currentFrame + frames, std::max(0, m_totalFrames));
        composeCurrentFrame();
    }

    bool isActive() const {
        return m_work != nullptr && m_currentFrame < m_totalFrames;
    }

    bool isFinished() const {
        return m_work == nullptr || m_currentFrame >= m_totalFrames;
    }

    int currentFrame() const {
        return std::max(0, m_currentFrame);
    }

    int totalFrames() const {
        return m_totalFrames;
    }

    Type type() const {
        return m_type;
    }

    bool render(SDL_Renderer* renderer, const SDL_Rect* destination = nullptr) {
        if (!renderer || !m_texture || !m_work) {
            return false;
        }

        if (SDL_UpdateTexture(m_texture, nullptr, m_work->pixels, m_work->pitch) != 0) {
            return false;
        }

        return SDL_RenderCopy(renderer, m_texture, nullptr, destination) == 0;
    }

private:
    static constexpr int kRandomBlockSize = 4;

    static SDL_Surface* copyAsArgb32(const SDL_Surface* surface) {
        if (!surface) {
            return nullptr;
        }
        return SDL_ConvertSurfaceFormat(const_cast<SDL_Surface*>(surface), SDL_PIXELFORMAT_ARGB8888, 0);
    }

    static SDL_Surface* createBlackLike(const SDL_Surface* reference) {
        if (!reference) {
            return nullptr;
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, reference->w, reference->h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface) {
            return nullptr;
        }
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
        return surface;
    }

    static int clampFrameIndex(int value, int totalFrames) {
        return std::clamp(value, 0, std::max(0, totalFrames - 1));
    }

    static uint32_t pixelAt(const SDL_Surface* surface, int x, int y) {
        const uint8_t* row = static_cast<const uint8_t*>(surface->pixels) + y * surface->pitch;
        return *(reinterpret_cast<const uint32_t*>(row) + x);
    }

    static void setPixel(SDL_Surface* surface, int x, int y, uint32_t pixel) {
        if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
            return;
        }
        uint8_t* row = static_cast<uint8_t*>(surface->pixels) + y * surface->pitch;
        *(reinterpret_cast<uint32_t*>(row) + x) = pixel;
    }

    static void fillBlack(SDL_Surface* surface) {
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
    }

    static void blitRect(SDL_Surface* dst,
                         int dx,
                         int dy,
                         SDL_Surface* src,
                         int sx,
                         int sy,
                         int sw,
                         int sh) {
        if (!dst || !src || sw <= 0 || sh <= 0) {
            return;
        }

        SDL_Rect srcRect = {sx, sy, sw, sh};
        SDL_Rect dstRect = {dx, dy, sw, sh};

        if (srcRect.x < 0) {
            dstRect.x -= srcRect.x;
            srcRect.w += srcRect.x;
            srcRect.x = 0;
        }
        if (srcRect.y < 0) {
            dstRect.y -= srcRect.y;
            srcRect.h += srcRect.y;
            srcRect.y = 0;
        }
        if (srcRect.x + srcRect.w > src->w) {
            srcRect.w = src->w - srcRect.x;
        }
        if (srcRect.y + srcRect.h > src->h) {
            srcRect.h = src->h - srcRect.y;
        }
        if (dstRect.x < 0) {
            srcRect.x -= dstRect.x;
            srcRect.w += dstRect.x;
            dstRect.x = 0;
        }
        if (dstRect.y < 0) {
            srcRect.y -= dstRect.y;
            srcRect.h += dstRect.y;
            dstRect.y = 0;
        }
        if (dstRect.x + srcRect.w > dst->w) {
            srcRect.w = dst->w - dstRect.x;
        }
        if (dstRect.y + srcRect.h > dst->h) {
            srcRect.h = dst->h - dstRect.y;
        }
        if (srcRect.w <= 0 || srcRect.h <= 0) {
            return;
        }

        dstRect.w = srcRect.w;
        dstRect.h = srcRect.h;
        SDL_BlitSurface(src, &srcRect, dst, &dstRect);
    }

    void prepareTransitionState(uint32_t seed) {
        m_randomBlocks.clear();
        m_mosaicRandomOffset.clear();

        if (!m_from) {
            return;
        }

        std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);

        auto initRandomBlocks = [&]() {
            const int blockColumns = m_from->w / kRandomBlockSize;
            const int blockRows = m_from->h / kRandomBlockSize;
            m_randomBlocks.resize(blockColumns * blockRows);
            for (size_t i = 0; i < m_randomBlocks.size(); ++i) {
                m_randomBlocks[i] = static_cast<uint32_t>(i);
            }
        };

        switch (m_type) {
            case Type::RandomBlocks:
                initRandomBlocks();
                std::shuffle(m_randomBlocks.begin(), m_randomBlocks.end(), rng);
                break;
            case Type::RandomBlocksDown:
            case Type::RandomBlocksUp: {
                initRandomBlocks();
                if (m_type == Type::RandomBlocksUp) {
                    std::reverse(m_randomBlocks.begin(), m_randomBlocks.end());
                }

                const int blockColumns = m_from->w / kRandomBlockSize;
                const int blockRows = m_from->h / kRandomBlockSize;
                const int length = 10;
                for (int i = 0; i < blockRows - 1; ++i) {
                    const int endIndex = (i < length ? 2 * i + 1 : (i <= blockRows - length ? i + length : (i + blockRows) / 2)) * blockColumns;
                    const int safeEnd = std::clamp(endIndex, 0, static_cast<int>(m_randomBlocks.size()));
                    std::shuffle(m_randomBlocks.begin() + i * blockColumns, m_randomBlocks.begin() + safeEnd, rng);

                    const int beginIndex = std::min(i * blockColumns + (i % 2 == 0 ? 0 : 2), safeEnd);
                    const int midIndex = std::min(i * blockColumns + (i % 2 == 0 ? 1 : 3) + (i > blockRows * 2 / 3 ? 3 : 0), safeEnd);

                    if (beginIndex < midIndex && midIndex <= safeEnd) {
                        if (m_type == Type::RandomBlocksDown) {
                            std::partial_sort(m_randomBlocks.begin() + beginIndex,
                                              m_randomBlocks.begin() + midIndex,
                                              m_randomBlocks.begin() + safeEnd);
                        } else {
                            std::partial_sort(m_randomBlocks.begin() + beginIndex,
                                              m_randomBlocks.begin() + midIndex,
                                              m_randomBlocks.begin() + safeEnd,
                                              std::greater<uint32_t>());
                        }
                    }
                }
                break;
            }
            case Type::MosaicIn:
            case Type::MosaicOut:
                m_mosaicRandomOffset.resize(std::max(0, m_totalFrames));
                for (int i = 0; i < m_totalFrames; ++i) {
                    std::uniform_int_distribution<int> distribution(0, std::max(0, i));
                    m_mosaicRandomOffset[i] = distribution(rng);
                }
                break;
            default:
                break;
        }
    }

    void composeCurrentFrame() {
        if (!m_work || !m_from || !m_to) {
            return;
        }

        if (m_totalFrames <= 0 || m_type == Type::None) {
            SDL_BlitSurface(m_to, nullptr, m_work, nullptr);
            return;
        }

        SDL_LockSurface(m_work);
        SDL_LockSurface(m_from);
        SDL_LockSurface(m_to);

        const int frame = clampFrameIndex(m_currentFrame, m_totalFrames);
        const int tfOff = std::max(1, m_totalFrames - 1);

        switch (m_type) {
            case Type::FadeIn:
            case Type::FadeOut:
                drawFade(frame);
                break;
            case Type::RandomBlocks:
            case Type::RandomBlocksDown:
            case Type::RandomBlocksUp:
                drawRandomBlocks(frame, tfOff);
                break;
            case Type::BlindOpen:
            case Type::BlindClose:
                drawBlinds(frame);
                break;
            case Type::VerticalStripesIn:
            case Type::VerticalStripesOut:
                drawVerticalStripes(frame, tfOff);
                break;
            case Type::HorizontalStripesIn:
            case Type::HorizontalStripesOut:
                drawHorizontalStripes(frame, tfOff);
                break;
            case Type::BorderToCenterIn:
            case Type::BorderToCenterOut:
                drawBorderToCenter(frame, tfOff);
                break;
            case Type::CenterToBorderIn:
            case Type::CenterToBorderOut:
                drawCenterToBorder(frame, tfOff);
                break;
            case Type::ScrollUpIn:
            case Type::ScrollUpOut:
                drawScroll(0, -m_from->h * frame / tfOff, 0, m_from->h - m_from->h * frame / tfOff);
                break;
            case Type::ScrollDownIn:
            case Type::ScrollDownOut:
                drawScroll(0, m_from->h * frame / tfOff, 0, -m_from->h + m_from->h * frame / tfOff);
                break;
            case Type::ScrollLeftIn:
            case Type::ScrollLeftOut:
                drawScroll(-m_from->w * frame / tfOff, 0, m_from->w - m_from->w * frame / tfOff, 0);
                break;
            case Type::ScrollRightIn:
            case Type::ScrollRightOut:
                drawScroll(m_from->w * frame / tfOff, 0, -m_from->w + m_from->w * frame / tfOff, 0);
                break;
            case Type::VerticalCombine:
            case Type::VerticalDivision:
                drawVerticalCombine(frame, tfOff);
                break;
            case Type::HorizontalCombine:
            case Type::HorizontalDivision:
                drawHorizontalCombine(frame, tfOff);
                break;
            case Type::CrossCombine:
            case Type::CrossDivision:
                drawCrossCombine(frame, tfOff);
                break;
            case Type::ZoomIn:
            case Type::ZoomOut:
                drawZoom(frame, tfOff);
                break;
            case Type::MosaicIn:
            case Type::MosaicOut:
                drawMosaic(frame);
                break;
            case Type::WaveIn:
            case Type::WaveOut:
                drawWave(frame, tfOff);
                break;
            case Type::CutIn:
                SDL_BlitSurface(m_to, nullptr, m_work, nullptr);
                break;
            case Type::CutOut:
                SDL_BlitSurface(m_from, nullptr, m_work, nullptr);
                break;
            case Type::None:
                SDL_BlitSurface(m_to, nullptr, m_work, nullptr);
                break;
        }

        SDL_UnlockSurface(m_to);
        SDL_UnlockSurface(m_from);
        SDL_UnlockSurface(m_work);
    }

    void drawFade(int frame) {
        SDL_BlitSurface(m_from, nullptr, m_work, nullptr);
        const int denominator = std::max(1, m_totalFrames - 2);
        const uint8_t alpha = static_cast<uint8_t>(std::clamp(255 * (frame + 1) / denominator, 0, 255));
        SDL_SetSurfaceBlendMode(m_to, SDL_BLENDMODE_BLEND);
        SDL_SetSurfaceAlphaMod(m_to, alpha);
        SDL_BlitSurface(m_to, nullptr, m_work, nullptr);
        SDL_SetSurfaceAlphaMod(m_to, 255);
        SDL_SetSurfaceBlendMode(m_to, SDL_BLENDMODE_NONE);
    }

    void drawRandomBlocks(int frame, int tfOff) {
        SDL_BlitSurface(m_from, nullptr, m_work, nullptr);
        const size_t blocksToDraw = m_randomBlocks.size() * static_cast<size_t>(frame + 1) / static_cast<size_t>(tfOff);
        const int blockColumns = m_from->w / kRandomBlockSize;

        for (size_t i = 0; i < blocksToDraw && i < m_randomBlocks.size(); ++i) {
            const uint32_t block = m_randomBlocks[i];
            const int x = static_cast<int>(block % blockColumns) * kRandomBlockSize;
            const int y = static_cast<int>(block / blockColumns) * kRandomBlockSize;
            blitRect(m_work, x, y, m_to, x, y, kRandomBlockSize, kRandomBlockSize);
        }
    }

    void drawBlinds(int frame) {
        fillBlack(m_work);
        const int step = std::clamp((frame + 5) / 5, 0, 8);
        for (int y = 0; y < m_from->h; y += 8) {
            const int bandHeight = std::min(8, m_from->h - y);
            if (m_type == Type::BlindOpen) {
                blitRect(m_work, 0, y, m_from, 0, y, m_from->w, std::max(0, bandHeight - step));
                blitRect(m_work, 0, y + bandHeight - step, m_to, 0, y + bandHeight - step, m_from->w, std::min(step, bandHeight));
            } else {
                blitRect(m_work, 0, y + step, m_from, 0, y + step, m_from->w, std::max(0, bandHeight - step));
                blitRect(m_work, 0, y, m_to, 0, y, m_from->w, std::min(step, bandHeight));
            }
        }
    }

    void drawVerticalStripes(int frame, int tfOff) {
        fillBlack(m_work);
        for (int i = 0; i < tfOff - (frame + 1); ++i) {
            blitRect(m_work, 0, i * 6 + 3, m_from, 0, i * 6 + 3, m_from->w, 3);
            blitRect(m_work, 0, m_from->h - i * 6, m_from, 0, m_from->h - i * 6, m_from->w, 3);
        }
        for (int i = 0; i < frame + 1; ++i) {
            blitRect(m_work, 0, i * 6, m_to, 0, i * 6, m_from->w, 3);
            blitRect(m_work, 0, m_from->h - 3 - i * 6, m_to, 0, m_from->h - 3 - i * 6, m_from->w, 3);
        }
    }

    void drawHorizontalStripes(int frame, int tfOff) {
        fillBlack(m_work);
        for (int i = 0; i < tfOff - (frame + 1); ++i) {
            blitRect(m_work, i * 8 + 4, 0, m_from, i * 8 + 4, 0, 4, m_from->h);
            blitRect(m_work, m_from->w - i * 8, 0, m_from, m_from->w - i * 8, 0, 4, m_from->h);
        }
        for (int i = 0; i < frame + 1; ++i) {
            blitRect(m_work, i * 8, 0, m_to, i * 8, 0, 4, m_from->h);
            blitRect(m_work, m_from->w - 4 - i * 8, 0, m_to, m_from->w - 4 - i * 8, 0, 4, m_from->h);
        }
    }

    void drawBorderToCenter(int frame, int tfOff) {
        SDL_BlitSurface(m_to, nullptr, m_work, nullptr);
        const int dx = (m_from->w / 2) * frame / tfOff;
        const int dy = (m_from->h / 2) * frame / tfOff;
        const int sw = m_from->w - m_from->w * frame / tfOff;
        const int sh = m_from->h - m_from->h * frame / tfOff;
        blitRect(m_work, dx, dy, m_from, dx, dy, sw, sh);
    }

    void drawCenterToBorder(int frame, int tfOff) {
        SDL_BlitSurface(m_from, nullptr, m_work, nullptr);
        const int dx = m_from->w / 2 - (m_from->w / 2) * frame / tfOff;
        const int dy = m_from->h / 2 - (m_from->h / 2) * frame / tfOff;
        const int sw = m_from->w * frame / tfOff;
        const int sh = m_from->h * frame / tfOff;
        blitRect(m_work, dx, dy, m_to, dx, dy, sw, sh);
    }

    void drawScroll(int oldDx, int oldDy, int newDx, int newDy) {
        fillBlack(m_work);
        blitRect(m_work, oldDx, oldDy, m_from, 0, 0, m_from->w, m_from->h);
        blitRect(m_work, newDx, newDy, m_to, 0, 0, m_to->w, m_to->h);
    }

    void drawVerticalCombine(int frame, int tfOff) {
        fillBlack(m_work);
        const int verCf = (m_type == Type::VerticalCombine) ? (tfOff - frame) : frame;
        SDL_Surface* screen1 = (m_type == Type::VerticalCombine) ? m_to : m_from;
        SDL_Surface* screen2 = (m_type == Type::VerticalCombine) ? m_from : m_to;

        blitRect(m_work, 0, -(m_from->h / 2) * verCf / tfOff, screen1, 0, 0, m_from->w, m_from->h / 2);
        blitRect(m_work, 0, m_from->h / 2 + (m_from->h / 2) * verCf / tfOff, screen1, 0, m_from->h / 2, m_from->w, m_from->h / 2);
        blitRect(m_work,
                 0,
                 m_from->h / 2 - (m_from->h / 2) * verCf / tfOff,
                 screen2,
                 0,
                 m_from->h / 2 - (m_from->h / 2) * verCf / tfOff,
                 m_from->w,
                 m_from->h * verCf / tfOff);
    }

    void drawHorizontalCombine(int frame, int tfOff) {
        fillBlack(m_work);
        const int horCf = (m_type == Type::HorizontalCombine) ? (tfOff - frame) : frame;
        SDL_Surface* screen1 = (m_type == Type::HorizontalCombine) ? m_to : m_from;
        SDL_Surface* screen2 = (m_type == Type::HorizontalCombine) ? m_from : m_to;

        blitRect(m_work, -(m_from->w / 2) * horCf / tfOff, 0, screen1, 0, 0, m_from->w / 2, m_from->h);
        blitRect(m_work, m_from->w / 2 + (m_from->w / 2) * horCf / tfOff, 0, screen1, m_from->w / 2, 0, m_from->w / 2, m_from->h);
        blitRect(m_work,
                 m_from->w / 2 - (m_from->w / 2) * horCf / tfOff,
                 0,
                 screen2,
                 m_from->w / 2 - (m_from->w / 2) * horCf / tfOff,
                 0,
                 m_from->w * horCf / tfOff,
                 m_from->h);
    }

    void drawCrossCombine(int frame, int tfOff) {
        fillBlack(m_work);
        const int crossCf = (m_type == Type::CrossCombine) ? (tfOff - frame) : frame;
        SDL_Surface* screen1 = (m_type == Type::CrossCombine) ? m_to : m_from;
        SDL_Surface* screen2 = (m_type == Type::CrossCombine) ? m_from : m_to;

        const int halfW = m_from->w / 2;
        const int halfH = m_from->h / 2;

        blitRect(m_work, -(halfW * crossCf) / tfOff, -(halfH * crossCf) / tfOff, screen1, 0, 0, halfW, halfH);
        blitRect(m_work, halfW + (halfW * crossCf) / tfOff, -(halfH * crossCf) / tfOff, screen1, halfW, 0, halfW, halfH);
        blitRect(m_work, halfW + (halfW * crossCf) / tfOff, halfH + (halfH * crossCf) / tfOff, screen1, halfW, halfH, halfW, halfH);
        blitRect(m_work, -(halfW * crossCf) / tfOff, halfH + (halfH * crossCf) / tfOff, screen1, 0, halfH, halfW, halfH);

        blitRect(m_work,
                 halfW - (halfW * crossCf) / tfOff,
                 0,
                 screen2,
                 halfW - (halfW * crossCf) / tfOff,
                 0,
                 m_from->w * crossCf / tfOff,
                 halfH - (halfH * crossCf) / tfOff);
        blitRect(m_work,
                 halfW,
                 halfH - (halfH * crossCf) / tfOff,
                 screen2,
                 halfW,
                 halfH - (halfH * crossCf) / tfOff,
                 halfW - (halfW * crossCf) / tfOff,
                 m_from->h * crossCf / tfOff);
        blitRect(m_work,
                 halfW - (halfW * crossCf) / tfOff,
                 halfH + (halfH * crossCf) / tfOff,
                 screen2,
                 halfW - (halfW * crossCf) / tfOff,
                 halfH + (halfH * crossCf) / tfOff,
                 m_from->w * crossCf / tfOff,
                 halfH + (halfH * crossCf) / tfOff);
        blitRect(m_work,
                 0,
                 halfH - (halfH * crossCf) / tfOff,
                 screen2,
                 0,
                 halfH - (halfH * crossCf) / tfOff,
                 halfW - (halfW * crossCf) / tfOff,
                 m_from->h * crossCf / tfOff);
    }

    void drawZoom(int frame, int tfOff) {
        fillBlack(m_work);

        int zoomFrame = (m_type == Type::ZoomOut) ? (tfOff - frame) : frame;
        SDL_Surface* screen = (m_type == Type::ZoomOut) ? m_to : m_from;
        zoomFrame = std::min(zoomFrame, std::max(0, m_totalFrames - 2));

        const int lengths[2] = {m_from->w, m_from->h};
        int sourcePos[2] = {0, 0};
        int sourceSize[2] = {m_from->w, m_from->h};

        for (int axis = 0; axis < 2; ++axis) {
            const int zMin = lengths[axis] / 4;
            const int zMax = lengths[axis] * 3 / 4;
            const int center = axis == 0 ? m_zoomCenter.x : m_zoomCenter.y;

            sourcePos[axis] = std::clamp(center, zMin, zMax) * zoomFrame / tfOff;
            sourceSize[axis] = std::max(1, lengths[axis] * (tfOff - zoomFrame) / tfOff);

            const int edgeFrame = (center < zMin)
                ? (tfOff * center / std::max(1, zMin) - tfOff)
                : (center > zMax)
                    ? (tfOff * (center - zMax) / std::max(1, lengths[axis] - zMax))
                    : 0;

            if (edgeFrame != 0 && zoomFrame > 0) {
                const int fixedPos = sourcePos[axis] * std::abs(edgeFrame) / zoomFrame;
                const int fixedSize = lengths[axis] * (tfOff - std::abs(edgeFrame)) / tfOff;
                sourcePos[axis] += zoomFrame < std::abs(edgeFrame)
                    ? ((edgeFrame > 0 ? 1 : 0) * (lengths[axis] - sourceSize[axis]) - sourcePos[axis])
                    : (edgeFrame > 0 ? lengths[axis] - fixedPos - fixedSize : -fixedPos);
            }
        }

        SDL_Rect srcRect = {
            std::clamp(sourcePos[0], 0, m_from->w - 1),
            std::clamp(sourcePos[1], 0, m_from->h - 1),
            std::clamp(sourceSize[0], 1, m_from->w),
            std::clamp(sourceSize[1], 1, m_from->h)
        };
        if (srcRect.x + srcRect.w > m_from->w) {
            srcRect.w = m_from->w - srcRect.x;
        }
        if (srcRect.y + srcRect.h > m_from->h) {
            srcRect.h = m_from->h - srcRect.y;
        }

        SDL_BlitScaled(screen, &srcRect, m_work, nullptr);
    }

    void drawMosaic(int frame) {
        fillBlack(m_work);

        int mosaicSize = frame + 1;
        int randomOffset = 0;
        SDL_Surface* screen = m_from;

        if (m_type == Type::MosaicIn) {
            mosaicSize = m_totalFrames - frame;
            screen = m_to;
            randomOffset = m_mosaicRandomOffset[std::clamp(m_totalFrames - frame - 1, 0, static_cast<int>(m_mosaicRandomOffset.size()) - 1)];
        } else if (!m_mosaicRandomOffset.empty()) {
            randomOffset = m_mosaicRandomOffset[std::clamp(frame, 0, static_cast<int>(m_mosaicRandomOffset.size()) - 1)];
        }

        mosaicSize = std::max(1, mosaicSize);
        const int offset = mosaicSize / 2;

        for (int row = 0; row < m_from->h + randomOffset; ++row) {
            const int srcRow = std::clamp(((row + offset) / mosaicSize) * mosaicSize - offset, 0, m_from->h - 1);
            for (int col = 0; col < m_from->w + randomOffset; ++col) {
                const int srcCol = std::clamp(((col + offset) / mosaicSize) * mosaicSize - offset, 0, m_from->w - 1);
                setPixel(m_work, col - randomOffset, row - randomOffset, pixelAt(screen, srcCol, srcRow));
            }
        }
    }

    void drawWave(int frame, int tfOff) {
        fillBlack(m_work);

        const int p = (m_type == Type::WaveIn) ? (m_totalFrames - frame) : (frame + 1);
        SDL_Surface* screen = (m_type == Type::WaveIn) ? m_to : m_from;
        const int depth = p;
        const double phase = p * 5.0 * M_PI / tfOff + M_PI;

        for (int y = 0; y < screen->h; ++y) {
            const double sy = y * (2.0 * M_PI) / 32.0;
            const int xOffset = static_cast<int>(2.0 * depth * std::sin(phase + sy));
            blitRect(m_work, xOffset, y, screen, 0, y, screen->w, 1);
        }
    }

    Type m_type = Type::None;
    SDL_Surface* m_from = nullptr;
    SDL_Surface* m_to = nullptr;
    SDL_Surface* m_work = nullptr;
    SDL_Texture* m_texture = nullptr;

    int m_totalFrames = 0;
    int m_currentFrame = 0;
    SDL_Point m_zoomCenter = {0, 0};

    std::vector<uint32_t> m_randomBlocks;
    std::vector<int32_t> m_mosaicRandomOffset;
};