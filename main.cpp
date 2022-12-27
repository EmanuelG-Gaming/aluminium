#include <map>
#include <vector>
#include <cmath>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 640;

SDL_Renderer *renderer = nullptr;

// Cxxdroid functions
static SDL_Texture *load_texture(const char *path)
{
    SDL_Surface *img = IMG_Load(path);
    if (img == NULL)
    {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, img);
    SDL_FreeSurface(img);
    if (texture == NULL)
    {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        return NULL;
    }
    return texture;
}

namespace Projection {
    float cameraX, cameraY;
    void world_to_screen(float &outX, float &outY)
    {
        // SCREEN_WIDTH/HEIGHT are the relative positions
        outX = (int)(SCREEN_WIDTH / 2 + outX - cameraX); 
        outY = (int)(SCREEN_HEIGHT / 2 + outY - cameraY); 
    }
    void adjust_camera(float relativeX, float relativeY) {
        cameraX = relativeX;
        cameraY = relativeY;
    }
};

namespace Utils {
    // Insert utilities here...
    float clamp(float &value, float min, float max)
    {
        if (value < min) value = min;
        else if (value > max) value = max;
        
        return value;
    }
    float another_clamp(float &value, float min, float max)
    {
        return std::max(min, std::min(max, value));
    }
    float interp(float from, float to, float progress) {
        return progress * (to - from);
    }
    SDL_Rect get_viewport_rect()
    {
        return { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    }
    bool rectangle_collide(const SDL_Rect *r1, const SDL_Rect *r2)
    {
        SDL_bool intersect = SDL_HasIntersection(r1, r2);
        return intersect == SDL_TRUE ? true : false;
    }
};

struct Vec2f {
    float x, y;
    void set_zero() {
        x = 0;
        y = 0;
    }
    Vec2f from(Vec2f &other) {
        Vec2f result = { other.x, other.y };
        return result;
    }
    float dot_prod(Vec2f &other) {
        return x * other.x + y * other.y;
    }
    Vec2f perpendicular(int side) {
        int j = side >= 0 ? 1 : -1;
        
        Vec2f result; 
        result.x = j * y;
        result.y = -j * x;
        
        return result;
    }  
    float len() {
        return sqrt(x*x + y*y);
    }
    float len2() {
        return x*x + y*y;
    }
    float dst(Vec2f &other) {
        float dx = x - other.x;
        float dy = y - other.y;
        
        return sqrt(dx * dx + dy * dy);
    }
    float dst2(Vec2f &other) {
        float dx = x - other.x;
        float dy = y - other.y;
        
        return dx * dx + dy * dy;
    }
    void multiply(float scalar) {
        x *= scalar;
        y *= scalar;
    }
    void norm() {
        multiply(1 / len());
    }
    Vec2f subtract(Vec2f other) {
        x -= other.x;
        y -= other.y;
        
        return *this;
    }
    Vec2f interpolate(Vec2f &other, float progress) {
        x = x + (other.x - x) * progress;
        y = y + (other.y - y) * progress;
        
        return *this;
    }
};

enum LoadStages{
    TEXTURES
};

class Assets {
    std::map<const char*, SDL_Texture*> textures;
    public:
        static Assets &get()
        {
            static Assets ins;
            return ins;
        }
        
        SDL_Texture *find_texture(const char *location) {
            return textures[location];
        }
        void add_texture(const char *location, const char *name) {
            SDL_Texture *t = load_texture(name);
            textures[location] = t;
        }
        void load(LoadStages stage);
    private:
        Assets() {}
        ~Assets() {}
    public:
        Assets(Assets const&) = delete;
        void operator = (Assets const&) = delete;    
};
void Assets::load(LoadStages stage) {
    switch (stage) {
         case TEXTURES:
              add_texture("aluminium-ball", "aluminium-ball.png");
              add_texture("wooden-ball", "wooden-ball.png");
              break;
    }
};

namespace Vars {
    Vec2f gravity = { 0.0f, 9.8f };
};

struct CollisionData {
    // This doesn't have to be exactly inside the object to collide with
    Vec2f intersection_point;
    bool collided;
};

struct Circle {
    Vec2f position;
    float radius;
};


namespace Draw {
    // Insert drawing methods here...
    void color(float r, float g, float b) {
        float ar = r * 255;
        float ag = g * 255; 
        float ab = b * 255;
        
        Utils::clamp(ar, 0, 255);
        Utils::clamp(ag, 0, 255);
        Utils::clamp(ab, 0, 255); 
        SDL_SetRenderDrawColor(renderer, (int) ar, (int) ag, (int) ab, 255);
    };
    void texture(SDL_Texture *tex, int x, int y, int w, int h)
    { 
        int sw = w;
        int sh = h;
        SDL_Rect cRect = {(int) x - sw / 2, (int) y - sh / 2, sw, sh};
        SDL_Rect v = Utils::get_viewport_rect();
        if (Utils::rectangle_collide(&cRect, &v))
            SDL_RenderCopy(renderer, tex, NULL, &cRect);
    }
    void rect_fill_uncentered(int x, int y, int w, int h)
    {
        SDL_Rect dest = { x, y, w, h };
        SDL_RenderFillRect(renderer, &dest);
    } 
    void rect_fill(int x, int y, int w, int h)
    {
        SDL_Rect dest = { x - w / 2, y - h / 2, w, h };
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) 
            SDL_RenderFillRect(renderer, &dest);
    }
    void line(int x1, int y1, int x2, int y2)
    {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
};
struct Line {
    Vec2f v1, v2;
    Vec2f gradient, normal;
    int side = 0;  
    float appliedMass = 4;
       
    void update(float timeTook);
    void render();
    CollisionData collision_circle(Circle c);
};
void Line::update(float timeTook) {
    gradient.x = v2.x - v1.x;
    gradient.y = v2.y - v1.y;
    normal = gradient.perpendicular(side);
    normal.norm();
};

CollisionData Line::collision_circle(Circle c) {
    float dx = v2.x - v1.x;
    float dy = v2.y - v1.y;
    float dx2 = c.position.x - v1.x;
    float dy2 = c.position.y - v1.y;
    Vec2f vec1 = { dx, dy };
    Vec2f vec2 = { dx2, dy2 };
    
    float len = vec1.len2();
    float dotProduct = vec1.dot_prod(vec2);
    float alpha = Utils::another_clamp(dotProduct, 0, len) / len;
    
    Vec2f interp_point = v1;
    interp_point.interpolate(v2, alpha);
    
    float dst = interp_point.dst(c.position);
    bool collided = dst <= c.radius;
    
    CollisionData data = { interp_point, collided };
    return data; 
};
 
void Line::render() {
    float dx = v1.x, dy = v1.y;
    float dx2 = v2.x, dy2 = v2.y;
    
    Projection::world_to_screen(dx, dy);
    Projection::world_to_screen(dx2, dy2);
    
    Draw::line(dx, dy, dx2, dy2);
};

class Ball {
    Circle source;
    
    float resistance;
    SDL_Texture *ballTexture;
    public:
       int index;
       Vec2f vel, acceleration;
       float mass;
       Line *colliding = nullptr;
       Ball(SDL_Texture *texture, float radius, float mass) {
           this->source.position.set_zero();
           this->vel.set_zero();
           this->acceleration.set_zero();
           
           this->source.radius = radius;
           
           this->ballTexture = texture;
           this->mass = mass;
           this->resistance = 0.75;
           this->index = 0;
       }
       void position(float x, float y) {
           source.position.x = x;
           source.position.y = y;
       }
       Circle get_circle() {
           return source;
       }
       
       void jump(float force, Line line);
       void moveX(float x);
       void moveY(float y);
       
       CollisionData collision(Ball *b);
       void update(float timeTook);
       void render();
};
void Ball::jump(float force, Line line) {
    Vec2f normal = line.normal;
    vel.x += normal.x * vel.y;
    vel.y += -force + normal.y;
};
void Ball::moveX(float x) {
    source.position.x += x;
};
void Ball::moveY(float y) {
    source.position.y += y;
};
void Ball::update(float timeTook) {
    // Ball kinematics
    acceleration.x = -vel.x * resistance + Vars::gravity.x * 60;
    acceleration.y = -vel.y * resistance + Vars::gravity.y * 60;
    
    vel.x += acceleration.x * timeTook;
    vel.y += acceleration.y * timeTook;
    
    source.position.x += vel.x * timeTook;
    source.position.y += vel.y * timeTook;
    
    if (fabs(vel.len2()) < 0.01f) {
        vel.set_zero();
    }
};
CollisionData Ball::collision(Ball *b) {
    Circle c1 = source;
    Circle c2 = b->get_circle();
    
    float dst = c1.position.dst2(c2.position);
    bool intersecting = dst <= (c1.radius + c2.radius) * (c1.radius + c2.radius);
    
    Vec2f n = { 0, 0 };
    CollisionData data = { n, intersecting };
    return data;
};

void Ball::render() {
    float ox = source.position.x, oy = source.position.y;
    Projection::world_to_screen(ox, oy);
    
    Draw::texture(ballTexture, ox, oy, source.radius * 2, source.radius * 2);
};

class Game
{
   public:
      const char *displayName = "";
      virtual ~Game() {};
      virtual void init() {};
      virtual void load() {};
    
      virtual void handle_event(SDL_Event ev) {};

      virtual void update(float timeTook) = 0;
};

class Aluminium : public Game {
    Ball *player;
    std::vector<Ball*> balls;
    std::vector<Line> lines;
    //Vec2f point;
    public:
       void init() override {
           displayName = "Aluminium";
       } 
       void load() override {
           Assets::get().load(TEXTURES);
           
           
           add_line(0, 0, 2000, 0, -1);
           add_line(0, 0, 0, -900);
           add_line(2000, 0, 2000, -900, -1);
           add_line(1000, 0, 2000, -500, -1);
           add_line(0, -500, 1800, -500);
           
           add_ball(50, -100, "aluminium-ball", 16, 1.7, true);
           for (int i = 0; i < 10; i++) {
                add_ball(1000 + i * 25, -250, "wooden-ball", 20, 2);
           }
           for (int i = 0; i < 30; i++) {
                add_ball(200 + i * 15, -250 - 500, "wooden-ball", 10, 0.4);
           }
           
       }
      
       void handle_event(SDL_Event ev) override {
           int cx = 0, cy = 0;
           SDL_GetMouseState(&cx, &cy);
           
           float f = cx > SCREEN_WIDTH / 2 ? 3 : -3;
           Utils::clamp(f, -10, 10);
           player->vel.x += f;
           
           Circle c = player->get_circle();
           if (player->colliding != nullptr) {
               if (cy < SCREEN_WIDTH / 2) {  
                   player->jump(300, *player->colliding);
                   player->colliding = nullptr;
               }
           }
       }
       void update(float timeTook) override {
           //point.set_zero(); 
           for (auto &line : lines) {
                line.update(timeTook);
           }
           for (auto &ball : balls) {
                ball->update(timeTook);
           }
           Circle c = player->get_circle();
           Projection::adjust_camera(c.position.x, c.position.y);
           
           // Collision detection
           for (auto &ball : balls) {
                Circle c1 = ball->get_circle();
                for (auto &line : lines) {
                     CollisionData dat = line.collision_circle(c1);
                     Vec2f intersection = dat.intersection_point;   
                     if (dat.collided) {
                         // Calculate distance
                         float dst = c1.position.dst(intersection);
                         float d = c1.radius - dst;
                    
                         ball->moveX(-d * (intersection.x - c1.position.x) / dst);
                         ball->moveY(-d * (intersection.y - c1.position.y) / dst);
                    
                         // Elastic collision
                         Vec2f nor = line.normal;
                    
                         float dotP = nor.dot_prod(ball->vel);
                         float j = 2 * dotP / (ball->mass + line.appliedMass);
                    
                         ball->vel.x = ball->vel.x - j * nor.x * line.appliedMass;
                         ball->vel.y = ball->vel.y - j * nor.y * line.appliedMass;
                       
                         ball->colliding = &line;
                     }
                }
                for (auto &other : balls) {
                     if (ball->index != other->index) {
                          Circle c2 = other->get_circle();      
                          CollisionData dat = ball->collision(other);
                          if (dat.collided) {
                               // Calculate distance again
                               float dst = c1.position.dst(c2.position);
                               float d = dst - c1.radius - c2.radius;
                               d *= 0.5;
                               
                               float bx1 = c1.position.x;
                               float by1 = c1.position.y;
                               
                               float bx2 = c2.position.x;
                               float by2 = c2.position.y;
                                
                               ball->moveX(-d * (bx1 - bx2) / dst);
                               ball->moveY(-d * (by1 - by2) / dst);
                               
                               other->moveX(d * (bx1 - bx2) / dst);
                               other->moveY(d * (by1 - by2) / dst);
                               
                               // Elastic collision
                               Vec2f gradient = { c2.position.x - c1.position.x, c2.position.y - c1.position.y };
                               Vec2f gradientVelocity = { ball->vel.x - other->vel.x, ball->vel.y - other->vel.y };  
                               Vec2f nor = gradient;
                               nor.norm();
                               
                               float dotP = nor.dot_prod(gradientVelocity);
                               float j = 2 * dotP / (ball->mass + other->mass);
                    
                               ball->vel.x = ball->vel.x - j * nor.x * other->mass;
                               ball->vel.y = ball->vel.y - j * nor.y * other->mass;
                               
                               other->vel.x = other->vel.x + j * nor.x * ball->mass;
                               other->vel.y = other->vel.y + j * nor.y * ball->mass;
                          }
                     }
                }
           }
           
           // Rendering
           Draw::color(0.1, 0.1, 0.85);
           Draw::rect_fill_uncentered(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
           Draw::color(1.0, 1.0, 1.0);
           for (auto &line : lines) {
                line.render();
           }
           for (auto &ball : balls) {
                ball->render();
           }
       }
       void add_line(float x1, float y1, float x2, float y2) {
           add_line(x1, y1, x2, y2, 0);
       }
       void add_line(float x1, float y1, float x2, float y2, int pointing) {
           Line line;
           line.v1 = { x1,  y1 };
           line.v2 = { x2, y2 };
           line.side = pointing;
           lines.push_back(line);
       }
       void add_ball(float x, float y, const char *spriteName, float radius, float mass) {
           add_ball(x, y, spriteName, radius, mass, false);
       }
       void add_ball(float x, float y, const char *spriteName, float radius, float mass, bool isPlayer) {
           Ball *ball = new Ball(Assets::get().find_texture(spriteName), radius, mass);
           ball->position(x, y);
           if (isPlayer) {
               player = ball;
           }
           ball->index = balls.size();
           balls.push_back(ball);
       }
};


int main()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    Aluminium game;
    game.init();
    
    SDL_Window *window = SDL_CreateWindow(game.displayName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (window == NULL)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }
    game.load();
    
    float then = 0.0f, delta = 0.0f;
    bool disabled = false;
    SDL_Event e;
    while (!disabled)
    {
        // Code cited from lazyfoo.net
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
                case SDL_QUIT:
                    disabled = true;
                    break;
            }
            game.handle_event(e);
        }
        float now = SDL_GetTicks();
        delta = (now - then) * 1000 / SDL_GetPerformanceFrequency();
        then = now;
        
        Draw::color(0, 0, 0);
        SDL_RenderClear(renderer);

        Draw::color(1, 1, 1);
        game.update(delta);

        SDL_RenderPresent(renderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}