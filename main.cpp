#include <SFML/Graphics.hpp>

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
void renderMandelbrot(sf::Image& image, 
    int width, 
    int height, 
    double minReal, 
    double maxReal, 
    double minImag, 
    double maxImag, 
    int maxIterations){
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            double offsets[4][2] = {
                {0.25, 0.25},
                {0.75, 0.25},
                {0.25, 0.75},
                {0.75, 0.75}
            };

            int totalR = 0;
            int totalG = 0;
            int totalB = 0;

            for (int i = 0; i < 4; i++) {
                double sampleX = x + offsets[i][0];
                double sampleY = y + offsets[i][1];

                double cx = minReal + sampleX / width * (maxReal - minReal);
                double cy = minImag + sampleY / height * (maxImag - minImag);

                int iteration = mandelbrot(cx, cy, maxIterations);
                sf::Color color = getColor(iteration, maxIterations);

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
    constexpr int WIDTH = 1920, HEIGHT = 1080, MAX_ITERATIONS = 1000;

    double minReal = -2.5;
    double maxReal = 1.0;
    double minImag = -1.5;
    double maxImag = 1.5;

    bool needsRedraw = true;


    sf::Image image;
    image.create(WIDTH, HEIGHT, sf::Color::Black);

    sf::Texture texture;
    sf::Sprite sprite(texture);

  

   

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Mandelbrot Explorer");
    window.setFramerateLimit(60);
 

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed) {
                double realRange = maxReal - minReal;
                double imagRange = maxImag - minImag;
                double moveAmountReal = realRange * 0.1;
                double moveAmountImag = imagRange * 0.1;

                if (event.key.code == sf::Keyboard::Left) {
                    minReal -= moveAmountReal;
                    maxReal -= moveAmountReal;
                    needsRedraw = true;
                }
                else if (event.key.code == sf::Keyboard::Right) {
                    minReal += moveAmountReal;
                    maxReal += moveAmountReal;
                    needsRedraw = true;
                }
                else if (event.key.code == sf::Keyboard::Up){
                    minImag -= moveAmountImag;
                    maxImag -= moveAmountImag;
                    needsRedraw = true;
                }
                else if (event.key.code == sf::Keyboard::Down) {
                    minImag += moveAmountImag;
                    maxImag += moveAmountImag;
                    needsRedraw = true;
                }
                else if (event.key.code == sf::Keyboard::Q) {
                    double zoomFactor = 0.5;
                    double centerReal = (minReal + maxReal) / 2.0;
                    double centerImag = (minImag + maxImag) / 2.0;
                    double newRealRange = realRange * zoomFactor;
                    double newImagRange = imagRange * zoomFactor;

                    minReal = centerReal - newRealRange / 2.0;
                    maxReal = centerReal + newRealRange / 2.0;
                    minImag = centerImag - newImagRange / 2.0;
                    maxImag = centerImag + newImagRange / 2.0;
                    needsRedraw = true;
                }
                else if (event.key.code == sf::Keyboard::E) {
                    double zoomFactor = 1.5;
                    double centerReal = (minReal + maxReal) / 2.0;
                    double centerImag = (minImag + maxImag) / 2.0;
                    double newRealRange = realRange * zoomFactor;
                    double newImagRange = imagRange * zoomFactor;

                    minReal = centerReal - newRealRange / 2.0;
                    maxReal = centerReal + newRealRange / 2.0;
                    minImag = centerImag - newImagRange / 2.0;
                    maxImag = centerImag + newImagRange / 2.0;
                    needsRedraw = true;
                }
            }
        }


        if (needsRedraw) {
            renderMandelbrot(image, WIDTH, HEIGHT, minReal, maxReal, minImag, maxImag, MAX_ITERATIONS);
            texture.loadFromImage(image);
            sprite.setTexture(texture, true);
            needsRedraw = false;
        }


        window.clear();
        window.draw(sprite);
        window.display();
    }
    return 0;
}