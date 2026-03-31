#include <SFML/Graphics.hpp>
#include <algorithm>

bool isInsideMainBulbs(double x, double y)
{
    if ((x + 1.0) * (x + 1.0) + y * y <= 0.0625)
    {
        return true;
    }

    double xMinusQuarter = x - 0.25;
    double q = xMinusQuarter * xMinusQuarter + y * y;

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

int mandelbrot(double cx, double cy, int maxIterations)
{
    if (isInsideMainBulbs(cx, cy))
    {
        return maxIterations;
    }

    double zx = 0.0;
    double zy = 0.0;
    int iteration = 0;

    while (zx * zx + zy * zy <= 4.0 && iteration < maxIterations)
    {
        double newZx = zx * zx - zy * zy + cx;
        double newZy = 2.0 * zx * zy + cy;

        zx = newZx;
        zy = newZy;
        iteration++;
    }

    return iteration;
}


struct ScreenData {
    static constexpr int WIDTH = 1920, HEIGHT = 1080;
    int maxIterations = 1000;
    
    double aspect = (double)WIDTH / HEIGHT;

    double minReal = -2.5;
    double maxReal = 1.0;
    double minImag = -1.5;
    double maxImag = 1.5;

    bool needsRedraw = true;

    double offsets[4][2] = {
                {0.25, 0.25},
                {0.75, 0.25},
                {0.25, 0.75},
                {0.75, 0.75}
    };


    double getRealRange() const {
        return maxReal - minReal;
    }

    double getImagRange() const {
        return maxImag - minImag;
    }

    double getCenterReal() const {
        return (minReal + maxReal) / 2.0;
    }

    double getCenterImag() const {
        return (minImag + maxImag) / 2.0;
    }
    void move(double realAmount, double imagAmount) {
        minReal += realAmount;
        maxReal += realAmount;
        minImag += imagAmount;
        maxImag += imagAmount;
        needsRedraw = true;
    }
    void zoom(double factor) {
        double realRange = getRealRange();
        double imagRange = getImagRange();

        double centerReal = getCenterReal();
        double centerImag = getCenterImag();

        double newRealRange = realRange * factor;
        double newImagRange = imagRange * factor;

        minReal = centerReal - newRealRange / 2.0;
        maxReal = centerReal + newRealRange / 2.0;
        minImag = centerImag - newImagRange / 2.0;
        maxImag = centerImag + newImagRange / 2.0;

        needsRedraw = true;
    }
    void applyBoxZoom(int left, int top, int width, int height) {
        double realRange = getRealRange();
        double imagRange = getImagRange();

        double newMinReal = minReal + (double)left / WIDTH * realRange;
        double newMaxReal = minReal + (double)(left + width) / WIDTH * realRange;
        double newMinImag = minImag + (double)top / HEIGHT * imagRange;
        double newMaxImag = minImag + (double)(top + height) / HEIGHT * imagRange;

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
    sf::Vector2f size = { 260.f, 180.f };

    bool isDraggingPanel = false;
    sf::Vector2f dragOffset = { 0.f, 0.f };

    int paletteIndex = 0;
};


void getCorrectedBox(
    sf::Vector2i start,
    sf::Vector2i end,
    int& left, int& top,
    int& width, int& height,
    double aspect)
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

void renderMandelbrot(sf::Image& image, 
   const ScreenData& d){
    for (int y = 0; y < d.HEIGHT; y++) {
        for (int x = 0; x < d.WIDTH; x++) {

            
            int totalR = 0;
            int totalG = 0;
            int totalB = 0;

            for (int i = 0; i < 4; i++) {
                double sampleX = x + d.offsets[i][0];
                double sampleY = y + d.offsets[i][1];

                double cx = d.minReal + sampleX / d.WIDTH * (d.maxReal - d.minReal);
                double cy = d.minImag + sampleY / d.HEIGHT * (d.maxImag - d.minImag);

                int iteration = mandelbrot(cx, cy, d.maxIterations);
                sf::Color color = getColor(iteration, d.maxIterations);

                totalR += color.r;
                totalG += color.g;
                totalB += color.b;
            }

            sf::Color averagedColor(
                totalR / 4,
                totalG / 4,
                totalB / 4
            );

            image.setPixel(x, y, averagedColor);
        }
    }
    }

int main() {
    ScreenData d;
    DragState s;
    PanelState p;

    sf::Image image;
    image.create(d.WIDTH, d.HEIGHT, sf::Color::Black);

    sf::Texture texture;
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

                double realRange = d.getRealRange();
                double imagRange = d.getImagRange();

                double moveAmountReal = d.getRealRange() * 0.1;
                double moveAmountImag = d.getImagRange() * 0.1;

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
            }

            //Mouse Controls
            else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    s.isDragging = true;
                    s.start = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
                    s.end = s.start;
                }
            }
            else if (event.type == sf::Event::MouseMoved) {
                if (s.isDragging) {
                    s.end = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
                }
            }
            else if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Left && s.isDragging) {
                    s.isDragging = false;
                    s.end = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);

                    double realRange = d.getRealRange();
                    double imagRange = d.getImagRange();
                    int top, left, width, height;

                    getCorrectedBox(s.start, s.end, left, top, width, height, (double)d.WIDTH / d.HEIGHT);

                    if (width > 5 && height > 5) {
                        d.applyBoxZoom(left, top, width, height);
                    }
                }
            }
        }
        
            


        if (d.needsRedraw) {
            renderMandelbrot(image, d);
            texture.loadFromImage(image);
            sprite.setTexture(texture, true);
            d.needsRedraw = false;
        }


        window.clear();
        window.draw(sprite);

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

        window.display();
    }
    return 0;
}