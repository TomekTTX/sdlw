#pragma once

#include "sdl.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <string_view>
#include <memory>
#include <functional>
#include <sstream>
#include <iomanip>

namespace sdlw {
    using Color = Uint32;
    enum class Font { ARIAL, SANS, COMIC_SANS, CONSOLAS, UBUNTU, WEBDINGS, WINGDINGS };

    /*struct Rect {
        int x, y, w, h;

        inline Rect translated(int dx, int dy) const { return { x + dx, y + dy, w, h }; }
        inline operator SDL_Rect() const { return { x,y,w,h }; }
    };*/

    inline SDL_Rect operator+(const SDL_Rect &rect, const SDL_Point &off) {
        return { rect.x + off.x, rect.y + off.y, rect.w, rect.h };
    }
    inline SDL_Point operator+(const SDL_Point &p1, const SDL_Point &p2) {
        return { p1.x + p2.x, p1.y + p2.y };
    }   
    inline SDL_Point operator-(const SDL_Point &p1, const SDL_Point &p2) {
        return { p1.x - p2.x, p1.y - p2.y };
    }

    class Graphics {
    private:
        bool valid;
        int w, h;
    public:
        SDL_Renderer *renderer;
        SDL_Surface *screen;
        SDL_Texture *scrtex;
        SDL_Window *window;

        Graphics(int w, int h);

        inline bool isValid() const { return valid; }

        inline Color color(int rgb) const {
            return SDL_MapRGB(screen->format,
                (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
        static TTF_Font *getFont(Font fontName, int fontSize);

        inline void clear() { SDL_FillRect(screen, NULL, 0x000000); }
        void drawPixel(int x, int y, Color color);
        void drawRect(SDL_Rect rect, Color color);
        void drawRect(SDL_Rect rect, int borderW, Color color, Color borderColor);
        void drawString(int x, int y, std::string_view text, int fontSize, Font fontName, Color color);
        void drawString(int x, int y, std::string_view text, TTF_Font *font, Color color);
        void drawString(SDL_Rect rect, std::string_view text, TTF_Font *font, Color color,
            bool hCenter = true, bool vCenter = true);

        ~Graphics();
    private:
        bool initItems(int w, int h);
        static SDL_Color sdlc(Color color);
    };

    class Component;

    class Window {
    private:
        using CompMap = std::unordered_map<std::string_view, std::unique_ptr<Component>>;
        enum class State { INIT, RUN, EXIT };

        int w, h;
        std::string_view title;
        CompMap components{};
        State state = State::INIT;
        Graphics g;
        TTF_Font *winfont;
        bool pendingUpdate = true;
    public:
        Window(int width, int height, std::string_view title,
            Font fontName = Font::CONSOLAS, int fontSize = 14);

        inline const Graphics &graphics() const { return g; }
        inline TTF_Font *font() const { return winfont; }

        Component *addComponent(std::unique_ptr<Component> &&comp, std::string_view id);
        inline Component *getComponent(std::string_view id) const {
            return components.count(id) ? components.at(id).get() : nullptr;
        }
        void run();

        virtual ~Window() {}
    private:
        void events();
        void draw();
        void update();
        bool handleEvent(const SDL_Event &event);
    };

    struct CompColors {
        Color bg{}, line{}, text{}, hl{}, extra1{}, extra2{}, extra3{};

        CompColors(std::initializer_list<int> colors) {
            auto fields = ptrs();
            int index = 0;
            for (int c : colors)
                *fields[index++] = c;
        }

        inline std::array<Color *, 7> ptrs() {
            return { &bg, &line, &text, &hl, &extra1, &extra2, &extra3 };
        }
    };

    class Component {
    protected:
        SDL_Rect rect;
        CompColors rawColors;
        CompColors colors{};
        Window *win{};
        bool hovered = false, shown = true;
    public:
        Component(SDL_Rect rect, const CompColors &colors) :
            rect(rect), rawColors(colors) {}

        inline int x() const { return rect.x; }
        inline int y() const { return rect.y; }
        inline int w() const { return rect.w; }
        inline int h() const { return rect.h; }
        inline SDL_Rect getRect() const { return rect; }
        inline bool isVisible() const { return shown; }

        inline bool posInside(SDL_Point pos) const { return SDL_PointInRect(&pos, &rect); }

        inline void show() { shown = true; }
        inline void hide() { shown = false; }
        inline void setVisibility(bool val) { shown = val; }
        inline void setDims(int w, int h) { rect.w = w; rect.h = h; }
        inline virtual void setWindow(Window *window) { win = window; }
        inline virtual void translate(int x, int y) { rect.x += x; rect.y += y; }
        inline void setPos(int x, int y) { translate(x - rect.x, y - rect.y); }
        inline void setColors(const CompColors &colors) { rawColors = colors; }
        void mapColors(const Graphics &g);

        virtual bool handleEvent(const SDL_Event &event) = 0;
        virtual void draw(Graphics &g) = 0;

        template <typename T>
        inline T *as() { return dynamic_cast<T *>(this); }

        virtual ~Component() {}
    protected:
        bool handleHoverHL(const SDL_Event &event);
        int thisWasClicked(const SDL_Event &event) const;
        int clickOutside(const SDL_Event &event) const;
    };

    class Panel : public Component {
    protected:
        std::vector<std::unique_ptr<Component>> comps;
    public:
        Panel(SDL_Rect rect, int bgcolor, int linecolor) :
            Component(rect, { bgcolor, linecolor }) {}

        inline std::size_t count() const { return comps.size(); }

        virtual bool handleEvent(const SDL_Event &event) override;
        virtual void draw(Graphics &g) override;
        virtual void translate(int x, int y) override;
        void setWindow(Window *window) override;

        virtual Component *addComponent(std::unique_ptr<Component> &&comp);
        inline Component *getComponent(std::size_t index) const {
            return index < comps.size() ? comps[index].get() : nullptr;
        }
        inline Component *operator[](std::size_t index) const {
            return getComponent(index);
        }
    };

    class ScrollPanel : public Panel {
    private:
        int index = 0, numShown;
        SDL_Point scrollBegin;
    public:
        ScrollPanel(SDL_Rect rect, int bgcolor, int linecolor,
            int numShown, SDL_Point scrollBegin) :
            Panel(rect, bgcolor, linecolor),
            numShown(numShown), scrollBegin(scrollBegin) {}
        ScrollPanel(SDL_Rect rect, int bgcolor, int linecolor, int numShown) :
            ScrollPanel(rect, bgcolor, linecolor, numShown, { rect.x, rect.y }) {}

        void translate(int x, int y) override;

        virtual bool handleEvent(const SDL_Event &event) override;
        Component *addComponent(std::unique_ptr<Component> &&comp) override;
    private:
        void scrollContent();
    };

    class Text : public Component {
    public:
        std::string text{};

        Text(SDL_Rect rect, std::string_view text, int color) :
            Component(rect, { 0,0,color }), text(text) {}

        virtual inline bool handleEvent(const SDL_Event &event) override { return false; }
        inline void draw(Graphics &g) override {
            if (shown) g.drawString(rect, text, win->font(), colors.text);
        }
    };

    class Button : public Component {
    private:
        std::function<void(Button *)> callback;
    public:
        std::string text{};

        Button(SDL_Rect rect, std::string_view text, const CompColors &colors,
            std::function<void(Button *)> callback) :
            Component(rect, colors), text(text), callback(std::move(callback)) {}

        virtual bool handleEvent(const SDL_Event &event) override;
        virtual void draw(Graphics &g) override;
    };

    class Expandable : public Component {
    public:
        enum class ExpandDir { UP, DOWN, LEFT_UP, RIGHT_UP, LEFT_DOWN, RIGHT_DOWN };
    protected:  
        bool expanded = false;
        std::string text;
        std::unique_ptr<Panel> panel;
        SDL_Point expOffset{};
    public:
        Expandable(SDL_Rect rect, std::string_view text, const CompColors &colors,
            std::unique_ptr<Panel> &&panel, ExpandDir expDir = ExpandDir::DOWN);

        inline Panel *getPanel() { return panel.get(); }

        inline void setExpanded(bool val) { panel->setVisibility(expanded = val); }
        inline void toggleExpanded() { setExpanded(!expanded); }
        void setExpandDir(ExpandDir dir);
        virtual void setWindow(Window *window) override;
        void translate(int x, int y) override;

        virtual bool handleEvent(const SDL_Event &event) override;
        virtual void draw(Graphics &g) override;
    protected:
        inline void adjustPanel() {
            panel->setPos(rect.x + expOffset.x, rect.y + expOffset.y);
        }
    };

    class ComboBox : public Expandable {
    private:
        class Elem : public Component {
        private:
            int ind;
            ComboBox *comboBox;
        public:
            std::string text;

            Elem(SDL_Rect rect, const CompColors &colors, int index,
                std::string_view text, ComboBox *parent) :
                Component(rect, colors), ind(index), text(text), comboBox(parent) {}

            inline int index() const { return ind; }

            virtual bool handleEvent(const SDL_Event &event) override;
            virtual void draw(Graphics &g) override;
        };
    private:
        int index = 0;
        std::vector<std::string_view> options;
    public:
        ComboBox(SDL_Rect rect, const std::vector<std::string_view> &options,
            const CompColors &colors, int numShown, ExpandDir expDir = ExpandDir::DOWN) :
            Expandable(rect, options[0], colors, makePanel(numShown), expDir),
            options(options) {
            if (win) finalizePanel();
        }

        inline int currentIndex() const { return index; }
        inline std::string_view currentText() const { return options[index]; }

        inline void setSelection(int ind) { index = ind; text = options[ind]; }
        void setWindow(Window *window) override;
    private:
        std::unique_ptr<ScrollPanel> makePanel(int numShown);
        void finalizePanel();
    };

    class Slider : public Component {
    public:
        using Callback = std::function<void(int)>;
    private:
        bool dragging = false, vertical;
        int min, max, step, lastVal{};
        float val;
        SDL_Rect sliderRect;
        SDL_Point mousePos{};
        Callback onValChange{};
    public:
        Slider(SDL_Rect rect, int min, int max,
            int step, const CompColors &colors,
            bool vertic = false, int slidRectWidth = 10) :
            Component(rect, colors), min(min), max(max),
            step(step), val(float(min)), vertical(vertic),
            sliderRect(makeSliderRect(slidRectWidth)) {}

        inline int trueVal() const { return stepn() * step + min; }
        inline int stepn() const { return static_cast<int>(std::round((val - min) / step)); }
        inline int valPxCount() const {
            return vertical ? h() - sliderRect.h : w() - sliderRect.w;
        }
        inline std::string str() const { return std::to_string(trueVal()); }

        inline void setCallback(Callback &&cb) { cb(trueVal()); onValChange = cb; }
        inline void setVal(int newVal) { val = float(newVal); dragDiff({ 0,0 }); }
        inline void setStepNo(int newStep) {
            val = min + 1.f * newStep * step; dragDiff({ 0,0 });
        }

        bool handleEvent(const SDL_Event &event) override;
        void draw(Graphics &g) override;
    private:  
        // pixels per step
        inline float pps() const { return 1.f * valPxCount() / (max - min) * step; }
        // units per pixel
        inline float upp() const { return 1.f * (max - min) / valPxCount(); }

        SDL_Rect makeSliderRect(int w) const;
        void checkCallback();
        void dragDiff(SDL_Point pdiff);
        void capVal();
    };

    class TextInput : public Component {
    public:
        using Callback = std::function<void(const std::string &)>;
    private:
        std::string text;
        int caretPos{};
        bool active = false, autoHide;
        Callback onConfirm{};
    public:
        TextInput(SDL_Rect rect, const CompColors &colors,
            std::string_view initVal = "", bool autoHide = false) :
            Component(rect, colors), text(initVal), autoHide(autoHide) {
            if (autoHide) hide();
        }

        void activate();
        void deactivate();
        inline void clear() { text = ""; caretPos = 0; }
        inline void setAutoHide(bool val = true) { autoHide = val; }
        inline void setCallback(Callback &&cb) { onConfirm = std::move(cb); }

        bool handleEvent(const SDL_Event &event) override;
        void draw(Graphics &g) override;

        void insertChar(char chr, std::size_t index);
        void deleteChar(std::size_t index);
    private:
        bool handleKey(int kp);
    };

    class ColorSelect : public Expandable {
    private:
        Slider *colSlider[3];
        std::unique_ptr<TextInput> input{};
    public:
        ColorSelect(SDL_Rect rect, const CompColors &colors,
            ExpandDir dir = ExpandDir::DOWN) :
            Expandable(rect, "", colors, makePanel(), dir),
            input(makeInput()) {}

        std::string str() const;
        Color color() const;

        void setColor(Color color);
        bool setColor(const std::string &hexStr);
        void setWindow(Window *window) override;

        bool handleEvent(const SDL_Event &event) override;
        void draw(Graphics &g) override;
    private:
        std::unique_ptr<Panel> makePanel() const;
        std::unique_ptr<TextInput> makeInput();
        void finalize();
        static std::string hex(char c);
    };
}
