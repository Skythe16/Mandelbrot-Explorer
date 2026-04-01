#include <SFML/Graphics.hpp>
#include <algorithm>
#include <thread>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <optional>
#include <iostream>
#include <ctime>
#include <filesystem>

using Real = long double;

bool isInsideMainBulbs(Real x, Real y)
{
    if ((x + 1.0) * (x + 1.0) + y * y <= 0.0625)
    {
        return true;
    }

    Real xMinusQuarter = x - 0.25;
    Real q = xMinusQuarter * xMinusQuarter + y * y;

    if (q * (q + xMinusQuarter) <= 0.25 * y * y)
    {
        return true;
    }

    return false;
}


sf::Color getColor(int iteration, int maxIterations) {
    if (iteration == maxIterations) {
        return sf::Color::Black;
    }

    double t = (double)iteration / maxIterations;

    sf::Uint8 r = static_cast<sf::Uint8>(9 * (1 - t) * t * t * t * 255);
    sf::Uint8 g = static_cast<sf::Uint8>(15 * (1 - t) * (1 - t) * t * t * 255);
    sf::Uint8 b = static_cast<sf::Uint8>(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);

    return sf::Color(r, g, b);
}

int mandelbrot(Real cx, Real cy, int maxIterations)
{
    if (isInsideMainBulbs(cx, cy))
    {
        return maxIterations;
    }

    Real zx = 0.0;
    Real zy = 0.0;
    int iteration = 0;

    while (zx * zx + zy * zy <= Real(4.0) && iteration < maxIterations)
    {
        Real newZx = zx * zx - zy * zy + cx;
        Real newZy = 2.0 * zx * zy + cy;

        zx = newZx;
        zy = newZy;
        iteration++;
    }

    return iteration;
}



struct ScreenData {
    static constexpr int WIDTH = 1920, HEIGHT = 1080;
    int maxIterations = 2000;
    
    Real aspect = (Real)WIDTH / HEIGHT;

    Real minReal = -2.5;
    Real maxReal = 1.0;
    Real minImag = -1.5;
    Real maxImag = 1.5;

    bool needsRedraw = true;

    static constexpr Real offsets[4][2] = {
                {0.25, 0.25},
                {0.75, 0.25},
                {0.25, 0.75},
                {0.75, 0.75}
    };


    Real getRealRange() const {
        return maxReal - minReal;
    }

    Real getImagRange() const {
        return maxImag - minImag;
    }

    Real getCenterReal() const {
        return (minReal + maxReal) / 2.0;
    }

    Real getCenterImag() const {
        return (minImag + maxImag) / 2.0;
    }
    void move(Real realAmount, Real imagAmount) {
        minReal += realAmount;
        maxReal += realAmount;
        minImag += imagAmount;
        maxImag += imagAmount;
        needsRedraw = true;
    }
    void zoom(Real factor) {
        Real realRange = getRealRange();
        Real imagRange = getImagRange();

        Real centerReal = getCenterReal();
        Real centerImag = getCenterImag();

        Real newRealRange = realRange * factor;
        Real newImagRange = imagRange * factor;

        minReal = centerReal - newRealRange / 2.0;
        maxReal = centerReal + newRealRange / 2.0;
        minImag = centerImag - newImagRange / 2.0;
        maxImag = centerImag + newImagRange / 2.0;

        needsRedraw = true;
    }
    void applyBoxZoom(int left, int top, int width, int height) {
        Real realRange = getRealRange();
        Real imagRange = getImagRange();

        Real newMinReal = minReal + (Real)left / WIDTH * realRange;
        Real newMaxReal = minReal + (Real)(left + width) / WIDTH * realRange;
        Real newMinImag = minImag + (Real)top / HEIGHT * imagRange;
        Real newMaxImag = minImag + (Real)(top + height) / HEIGHT * imagRange;

        minReal = newMinReal;
        maxReal = newMaxReal;
        minImag = newMinImag;
        maxImag = newMaxImag;

        needsRedraw = true;
    }
};

struct DragState {
    bool isDragging = false;
    sf::Vector2i start;
    sf::Vector2i end;
};

struct PanelState {
    sf::Vector2f position = { 20.f, 20.f };
    sf::Vector2f size = { 260.f, 220.f };

    bool isDraggingPanel = false;
    sf::Vector2f dragOffset = { 0.f, 0.f };

    int paletteIndex = 0;
    bool visible = true;
};


struct RenderState {
    std::thread worker;
    std::mutex mutex;

    std::atomic<bool> renderInProgress = false;
    std::atomic<bool> resultReady = false;
    std::atomic<bool> shutdown = false;

    std::vector<sf::Uint8> completedPixels;
    ScreenData pendingScreenData;
};



enum class ButtonAction {
    None,
    Reset,
    Screenshot
};

struct ControlPanel {
    struct Button {
        sf::RectangleShape shape;
        std::string label;
        sf::Text text;


        Button() = default;

        Button(const sf::Vector2f& position, const sf::Vector2f& size, const std::string& labelText, sf::Font& font)
            : label(labelText)
        {
            shape.setPosition(position);
            shape.setSize(size);
            shape.setFillColor(sf::Color(80, 80, 80));
            shape.setOutlineColor(sf::Color::White);
            shape.setOutlineThickness(1.f);

            text.setFont(font);
            text.setString(labelText);
            text.setCharacterSize(16);
            text.setFillColor(sf::Color::White);
        }
        sf::FloatRect getBounds() const {
            return shape.getGlobalBounds();
        }

        bool contains(const sf::Vector2f& mousePos) const {
            return shape.getGlobalBounds().contains(mousePos);
        }

        void setPosition(const sf::Vector2f& position) {
            shape.setPosition(position);
            updateTextPosition();
        }
        void updateTextPosition() {
            sf::FloatRect textBounds = text.getLocalBounds();
            sf::Vector2f buttonPos = shape.getPosition();
            sf::Vector2f buttonSize = shape.getSize();

            text.setPosition(
                buttonPos.x + (buttonSize.x - textBounds.width) / 2.f - textBounds.left,
                buttonPos.y + (buttonSize.y - textBounds.height) / 2.f - textBounds.top
            );
        }
        void setSize(const sf::Vector2f& size) {
            shape.setSize(size);
            updateTextPosition();
        }
    };
    struct Slider {
        sf::FloatRect bounds;
        float minValue;
        float maxValue;
        float value;
        bool dragging = false;
    };

    sf::RectangleShape panelShape;

    sf::FloatRect panelBounds;

    Button resetButton;
    Button screenshotButton;

    ControlPanel(PanelState& p, sf::Font& font) 
        : resetButton(sf::Vector2f(p.position.x + (p.size.x / 6.f), p.position.y + 40.f),
            sf::Vector2f(100.f, 30.f), 
            "Reset", font), screenshotButton(sf::Vector2f(p.position.x + 2 * (p.size.x / 6.f), p.position.y + 40.f),
        sf::Vector2f(100.f, 30.f),
        "Screenshot", font) {
        panelShape.setPosition(p.position);
        panelShape.setSize(p.size);
        panelShape.setFillColor(sf::Color(30, 30, 30, 200));
        panelShape.setOutlineColor(sf::Color::White);
        panelShape.setOutlineThickness(1.f);

        panelBounds = sf::FloatRect(p.position.x, p.position.y, p.size.x, p.size.y);



    }

    void update(const PanelState& p) {
        panelShape.setPosition(p.position);
        panelShape.setSize(p.size);

        panelBounds = sf::FloatRect(p.position.x, p.position.y, p.size.x, p.size.y);

        float padding = 20.f;
        float gap = 10.f;
        float buttonHeight = 30.f;
        float topOffset = 40.f;

        float contentWidth = p.size.x - 2.f * padding;
        float buttonWidth = (contentWidth - gap) / 2.f;

        resetButton.setSize({ buttonWidth, buttonHeight });
        screenshotButton.setSize({ buttonWidth, buttonHeight });

        // Button updates
        resetButton.setPosition({
        p.position.x + padding,
        p.position.y + topOffset
            });
        screenshotButton.setPosition({
        p.position.x +  padding + buttonWidth + gap,
        p.position.y + topOffset
            });
    }

    ButtonAction getClickedButton(const sf::Vector2f& mousePos) const {
        if (resetButton.contains(mousePos)) {
            return ButtonAction::Reset;
        }
        if (screenshotButton.contains(mousePos)) {
            return ButtonAction::Screenshot;
        }
        return ButtonAction::None;
    }

};





void renderRows(
    std::vector<sf::Uint8>& pixels,
    const ScreenData& d,
    int imageWidth,
    int imageHeight,
    int startY,
    int endY)
{
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < imageWidth; x++) {
            int totalR = 0;
            int totalG = 0;
            int totalB = 0;

            for (int i = 0; i < 4; i++) {
                Real sampleX = x + d.offsets[i][0];
                Real sampleY = y + d.offsets[i][1];

                Real cx = d.minReal + sampleX / imageWidth * d.getRealRange();
                Real cy = d.minImag + sampleY / imageHeight * d.getImagRange();

                int iteration = mandelbrot(cx, cy, d.maxIterations);
                sf::Color color = getColor(iteration, d.maxIterations);

                totalR += color.r;
                totalG += color.g;
                totalB += color.b;
            }

            int pixelIndex = 4 * (y * imageWidth + x);
            pixels[pixelIndex + 0] = static_cast<sf::Uint8>(totalR / 4);
            pixels[pixelIndex + 1] = static_cast<sf::Uint8>(totalG / 4);
            pixels[pixelIndex + 2] = static_cast<sf::Uint8>(totalB / 4);
            pixels[pixelIndex + 3] = 255;
        }
    }
}

std::vector<sf::Uint8> renderMandelbrot(const ScreenData& d)
{
    std::vector<sf::Uint8> pixels(d.WIDTH * d.HEIGHT * 4);

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) {
        threadCount = 4;
    }

    threadCount = std::max(1u, threadCount - 1);

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    int rowsPerThread = d.HEIGHT / threadCount;
    int currentStartY = 0;

    for (unsigned int i = 0; i < threadCount; i++) {
        int startY = currentStartY;
        int endY = (i == threadCount - 1) ? d.HEIGHT : startY + rowsPerThread;

        threads.emplace_back(
            renderRows,
            std::ref(pixels),
            std::cref(d),
            d.WIDTH,
            d.HEIGHT,
            startY,
            endY
        );
        currentStartY = endY;
    }

    for (auto& t : threads) {
        t.join();
    }

    return pixels;
}

std::vector<sf::Uint8> renderMandelbrotHighRes(
    const ScreenData& d,
    int imageWidth,
    int imageHeight)
{
    std::vector<sf::Uint8> pixels(imageWidth * imageHeight * 4);

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) {
        threadCount = 4;
    }

    threadCount = std::max(1u, threadCount - 1);

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    int rowsPerThread = imageHeight / threadCount;
    int currentStartY = 0;

    for (unsigned int i = 0; i < threadCount; i++) {
        int startY = currentStartY;
        int endY = (i == threadCount - 1) ? imageHeight : startY + rowsPerThread;

        threads.emplace_back(
            renderRows,
            std::ref(pixels),
            std::cref(d),
            imageWidth,
            imageHeight,
            startY,
            endY
        );

        currentStartY = endY;
    }

    for (auto& t : threads) {
        t.join();
    }

    return pixels;
}

void startBackgroundRender(RenderState& rs, const ScreenData& d)
{
    if (rs.renderInProgress) {
        return;
    }

    rs.renderInProgress = true;
    rs.resultReady = false;

    {
        std::lock_guard<std::mutex> lock(rs.mutex);
        rs.pendingScreenData = d;
    }

    rs.worker = std::thread([&rs]() {
        ScreenData localData;
        {
            std::lock_guard<std::mutex> lock(rs.mutex);
            localData = rs.pendingScreenData;
        }

        std::vector<sf::Uint8> pixels = renderMandelbrot(localData);

        {
            std::lock_guard<std::mutex> lock(rs.mutex);
            rs.completedPixels = std::move(pixels);
        }

        rs.resultReady = true;
        rs.renderInProgress = false;
        });
}

void joinWorkerIfFinished(RenderState& rs)
{
    if (rs.worker.joinable() && !rs.renderInProgress) {
        rs.worker.join();
    }
}

void getCorrectedBox(
    sf::Vector2i start,
    sf::Vector2i end,
    int& left, int& top,
    int& width, int& height,
    Real aspect)
{
    int dx = end.x - start.x;
    int dy = end.y - start.y;

    if (abs(dx) > abs(dy) * aspect) {
        dy = (int)(dx / aspect);
    }
    else {
        dx = (int)(dy * aspect);
    }

    int right = start.x + dx;
    int bottom = start.y + dy;

    left = std::min(start.x, right);
    top = std::min(start.y, bottom);

    width = abs(dx);
    height = abs(dy);
}



int main() {
    ScreenData d, refresh;
    DragState s;
    PanelState p;

    int sSCount = 0;

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/Times New Roman.ttf")) {
        std::cout << "Error loading font, try again.";
    }

    ControlPanel c(p, font);

    RenderState renderState;

  

    sf::Vector2f mousePos = { 0.f, 0.f };

    sf::Texture texture;
    texture.create(d.WIDTH, d.HEIGHT);
    sf::Sprite sprite(texture);

  

    sf::RenderWindow window(sf::VideoMode(d.WIDTH, d.HEIGHT), "Mandelbrot Explorer");
    window.setFramerateLimit(60);
 

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            //Key Controls
            if (event.type == sf::Event::KeyPressed) {

                Real realRange = d.getRealRange();
                Real imagRange = d.getImagRange();

                Real moveAmountReal = d.getRealRange() * 0.1;
                Real moveAmountImag = d.getImagRange() * 0.1;

                if (event.key.code == sf::Keyboard::Left) {
                    d.move(-moveAmountReal, 0);
                }
                else if (event.key.code == sf::Keyboard::Right) {
                    d.move(moveAmountReal, 0);
                }
                else if (event.key.code == sf::Keyboard::Up) {
                    d.move(0, -moveAmountImag);
                }
                else if (event.key.code == sf::Keyboard::Down) {
                    d.move(0, moveAmountImag);
                }
                else if (event.key.code == sf::Keyboard::Q) {
                    d.zoom(0.5);
                }
                else if (event.key.code == sf::Keyboard::E) {
                    d.zoom(1.5);
                }
                else if (event.key.code == sf::Keyboard::R) {
                    d = refresh;
                    d.needsRedraw = true;
                }
            }

            //Mouse Controls
            else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f mousePos(
                        static_cast<float>(event.mouseButton.x),
                        static_cast<float>(event.mouseButton.y)
                    );

                    if (c.panelBounds.contains(mousePos.x, mousePos.y)) {
                        ButtonAction action = c.getClickedButton(mousePos);

                        if (action == ButtonAction::Reset) {
                            d = refresh;
                            d.needsRedraw = true;
                        }
                        else if (action == ButtonAction::Screenshot) {
                            int exportWidth = 5120;
                            int exportHeight = 2880;

                            std::vector<sf::Uint8> exportPixels =
                                renderMandelbrotHighRes(d, exportWidth, exportHeight);

                            sf::Image image;
                            image.create(exportWidth, exportHeight, exportPixels.data());

                            std::filesystem::path dir = "screenshots";
                            if (!std::filesystem::exists(dir)) {
                                std::filesystem::create_directory(dir);
                            }

                            std::time_t t = std::time(nullptr);
                            std::string filename =
                                "screenshots/Screenshot_" + std::to_string(t) + "_" + std::to_string(sSCount++) + ".png";

                            if (image.saveToFile(filename)) {
                                std::cout << "High-res screenshot saved: " << filename << "\n";
                            }
                            else {
                                std::cout << "Failed to save screenshot\n";
                            }
                        }
                        else {
                            p.isDraggingPanel = true;
                            p.dragOffset = mousePos - p.position;
                        }
                    }
                    else {
                        s.isDragging = true;
                        s.start = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
                        s.end = s.start;
                    }
                }
            }
            else if (event.type == sf::Event::MouseMoved) {
                mousePos = sf::Vector2f(event.mouseMove.x, event.mouseMove.y);

                if (p.isDraggingPanel) {
                    p.position = mousePos - p.dragOffset;
                }else

                if (s.isDragging) {
                    s.end = sf::Vector2i(mousePos);
                }
            }
            else if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    if (p.isDraggingPanel) {
                        p.isDraggingPanel = false;
                    }
                    else if (s.isDragging) {
                        s.isDragging = false;
                        s.end = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);

                        int top, left, width, height;
                        getCorrectedBox(s.start, s.end, left, top, width, height, (Real)d.WIDTH / d.HEIGHT);

                        if (width > 5 && height > 5) {
                            d.applyBoxZoom(left, top, width, height);
                        }
                    }
                }
            }
        }
        
            


        if (d.needsRedraw && !renderState.renderInProgress) {
            startBackgroundRender(renderState, d);
            d.needsRedraw = false;
        }

        if (renderState.resultReady) {
            {
                std::lock_guard<std::mutex> lock(renderState.mutex);
                if (!renderState.completedPixels.empty()) {
                    texture.update(renderState.completedPixels.data());
                }
            }

            renderState.resultReady = false;
            joinWorkerIfFinished(renderState);
        }

// Drawing Block
        window.clear();

        //Fractal Drawing
        window.draw(sprite);

        //Drag-box Drawing
        if (s.isDragging) {

            int left, top, width, height;

            getCorrectedBox(s.start, s.end, left, top, width, height, d.aspect);

            sf::RectangleShape zoomBox;

            zoomBox.setPosition((float)left, (float) top);
            zoomBox.setSize(sf::Vector2f((float)(width), (float)(height)));

            zoomBox.setFillColor(sf::Color(255, 255, 255, 50));
            zoomBox.setOutlineColor(sf::Color::White);
            zoomBox.setOutlineThickness(1.0f);

            window.draw(zoomBox);

        }


        //Panel Drawing
        c.update(p);
        window.draw(c.panelShape);
        window.draw(c.resetButton.shape);
        window.draw(c.screenshotButton.shape);
        window.draw(c.resetButton.text);
        window.draw(c.screenshotButton.text);







        window.display();
    }

    if (renderState.worker.joinable()) {
        renderState.worker.join();
    }

    return 0;
}