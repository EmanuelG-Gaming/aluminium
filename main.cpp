#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
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
    float radians(float degrees) {
        return degrees / 180 * M_PI;
    }
    float degrees(float radians) {
        return radians * 180 / M_PI;
    }
    float f_sin(float a) {
        return sin(radians(a));
    }
    float f_cos(float a) {
        return cos(radians(a));
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
    float cross_prod(Vec2f &other) {
        return x * other.y - y * other.x;
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
    Vec2f subtract(Vec2f &other) {
        x -= other.x;
        y -= other.y;
        
        return *this;
    }
    Vec2f add(float ox, float oy) {
        x += ox;
        y += oy;
        
        return *this;
    }
    Vec2f interpolate(Vec2f &other, float progress) {
        x = x + (other.x - x) * progress;
        y = y + (other.y - y) * progress;
        
        return *this;
    }
    Vec2f rotate(float angle) {
        float mx = x;
        float my = y;
        
        x = mx * cos(angle) - my * sin(angle);
        y = mx * sin(angle) + my * cos(angle);
        
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
              add_texture("wooden-plank", "wooden-plank.png");
              add_texture("wooden-beam", "wooden-beam.png");
              break;
    }
};

namespace Vars {
    Vec2f gravity = { 0.0f, 9.8f };
    // Measured in radians
    float gravity_angle() {
        return atan2(-gravity.y, gravity.x);
    }
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
        int sw = (int) w;
        int sh = (int) h;
        SDL_Rect cRect = {(int) x - sw / 2, (int) y - sh / 2, sw, sh};
        SDL_Rect v = Utils::get_viewport_rect();
        if (Utils::rectangle_collide(&cRect, &v))
            SDL_RenderCopy(renderer, tex, NULL, &cRect);
    }
    void texture_uncentered(SDL_Texture *tex, int x, int y, int width, int height)
    { 
        int sw = (int) width;
        int sh = (int) height;
        SDL_Rect cRect = {x, y, sw, sh};
        SDL_Rect v = Utils::get_viewport_rect();
        if (Utils::rectangle_collide(&cRect, &v))
            SDL_RenderCopy(renderer, tex, NULL, &cRect);
    }
    void rotated_texture(SDL_Texture *tex, int x, int y, int width, int height, float angle)
    {
        int sw = (int) width;
        int sh = (int) height;
        SDL_Rect cRect = {x, y, sw, sh};
        SDL_RenderCopyEx(renderer, tex, NULL, &cRect, angle, NULL, SDL_FLIP_NONE);
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

class WorldObject {
    public:
        float resistance;
        float mass;
  
        Vec2f position;
        Vec2f vel, acceleration;
    
        WorldObject *colliding = nullptr;
        int index = 0;
        const char *name;
        WorldObject(float mass) {
            this->mass = mass;
            this->resistance = 0.85f;
            
            // Reset position, velocity, acceleration
            reset();
        }
        void place(float x, float y) {
             position.x = x;
             position.y = y;
        }
        void moveX(float x) {
             position.x += x;
        }
        void moveY(float y) {
             position.y += y;
        }
        void reset() {
             position.set_zero();
             vel.set_zero();
             acceleration.set_zero();
        } 
        virtual CollisionData collision(WorldObject *object) {}
        virtual void update(float timeTook) {}
        virtual void render() {}
};

class Ball : public WorldObject {
    SDL_Texture *ballTexture;
    public: 
        float radius;
        Ball(const char *spriteName, float radius, float mass) : WorldObject(mass) {
            this->radius = radius;
            this->ballTexture = Assets::get().find_texture(spriteName);
            this->name = "ball";
        }
        SDL_Texture *get_texture() {
            return ballTexture;
        }   
        void jump(float force, WorldObject *o);
        void update(float timeTook) override;
        void render() override;
    
    CollisionData collision(WorldObject *object) override;
};
class Line : public WorldObject {
    public:
        Vec2f endPosition;
        Vec2f gradient, normal;
        int side = 0;  
        Line(Vec2f v1, Vec2f v2) : WorldObject(4.0f) {
            this->position = v1;
            this->endPosition = v2;
            
            this->name = "line";
        }
        void update(float timeTook) override;
        void render() override;
};

class Rectangle : public WorldObject {
    SDL_Texture *rectangleTexture;
    public:
        float width;
        float height;
        float angle;
        Rectangle(const char *textureName, float width, float height, float angle) : WorldObject(4.0f) {
            this->width = width;
            this->height = height;
            this->angle = Utils::radians(angle);
            this->rectangleTexture = Assets::get().find_texture(textureName);
            
            this->name = "rectangle";
        }
        void render() override;
};
void Rectangle::render() {
     float ox = position.x, oy = position.y;
     Projection::world_to_screen(ox, oy);
     
     Draw::rotated_texture(rectangleTexture, ox, oy, width, height, Utils::degrees(angle));
};

void Ball::jump(float force, WorldObject *o) {
     const char *oName = o->name;
     
     if (oName == "line") {
          Vec2f normal = ((Line*) o)->normal;
          vel.x += normal.x * vel.y;
          vel.y += -force + normal.y;
     }
     if (oName == "rectangle") {
          Rectangle *r = (Rectangle*) o;
          Vec2f p = collision(r).intersection_point;
          Vec2f m = r->position;
                                      
          // Retransform the intersection point
          m.add(r->width / 2, r->height / 2);
          p.subtract(m);
          p.rotate(r->angle);
          p.add(m.x, m.y);
              
          Vec2f normal = p;
          normal.subtract(position);
          normal.norm();
            
          vel.x += normal.x * vel.y;
          vel.y += -force + normal.y;
     }
     if (oName == "ball") {
          float dx = o->position.x - position.x;
          float dy = o->position.y - position.y;
               
          float angle = atan2(dy, dx);
          float px = cos(angle) * force;
          float py = sin(angle) * force;
               
          vel.x -= px;
          vel.y -= py;
               
          o->vel.x += px;
          o->vel.y += py;   
     }
};
void Ball::update(float timeTook) {
    // Ball kinematics
    acceleration.x = -vel.x * resistance + Vars::gravity.x * 60;
    acceleration.y = -vel.y * resistance + Vars::gravity.y * 60;
    
    vel.x += acceleration.x * timeTook;
    vel.y += acceleration.y * timeTook;
    
    position.x += vel.x * timeTook;
    position.y += vel.y * timeTook;
    
    if (position.y >= radius + 50000) {
        place(position.x, -400);
    }
    if (fabs(vel.len2()) < 0.01f) {
        vel.set_zero();
    }
};
CollisionData Ball::collision(WorldObject *object) {
     CollisionData data;
     
     const char *oName = object->name;
     // Colliding with a line
     if (oName == "line") {
          Vec2f v1 = object->position;
          Vec2f v2 = ((Line*) object)->endPosition;
               
          float dx = v2.x - v1.x;
          float dy = v2.y - v1.y;
          float dx2 = position.x - v1.x;
          float dy2 = position.y - v1.y;
               
          Vec2f vec1 = { dx, dy };
          Vec2f vec2 = { dx2, dy2 };
    
          float len = vec1.len2();
          float dotProduct = vec1.dot_prod(vec2);
          float alpha = Utils::another_clamp(dotProduct, 0, len) / len;
    
          Vec2f interp_point = v1;
          interp_point.interpolate(v2, alpha);
     
          float dst = interp_point.dst2(position);
          bool collided = dst <= radius * radius;
               
          data.intersection_point = interp_point;
          data.collided = collided;
     }
     // Colliding with a rectangle
     if (oName == "rectangle") {
          Rectangle *dest = (Rectangle*) object;
          Vec2f centerRectangle = dest->position;
          centerRectangle.add(dest->width / 2, dest->height / 2);
          Vec2f centerBall = position;
          Vec2f intersection;
          
          Vec2f gradient = { centerBall.x - centerRectangle.x, centerBall.y - centerRectangle.y };
          Vec2f r = gradient;
          r.rotate(-dest->angle);
          r.add(centerRectangle.x, centerRectangle.y);
          
          float dx = dest->position.x;
          float dy = dest->position.y;
          if (r.x < dx) {
              intersection.x = dx;
          } else if (r.x > (dx + dest->width)) {
              intersection.x = dx + dest->width;
          }
          else intersection.x = r.x;
          
          if (r.y < dy) {
              intersection.y = dy;
          } else if (r.y > (dy + dest->height)) {
              intersection.y = dy + dest->height;
          }
          else intersection.y = r.y;
           
          Vec2f m = { r.x - intersection.x, r.y - intersection.y };
          data.collided = m.len2() <= radius * radius;
          data.intersection_point = intersection;
     }
     // Colliding with another ball   
     if (oName == "ball") {
          float dst = position.dst2(object->position);
          float r = ((Ball*) object)->radius;
          bool intersecting = dst <= (radius + r) * (radius + r);
    
          Vec2f n = { 0, 0 };
          data.intersection_point = n;
          data.collided = intersecting;
     }
     
     return data;
};

void Ball::render() {
    float ox = position.x, oy = position.y;
    Projection::world_to_screen(ox, oy);
    
    Draw::texture(ballTexture, ox, oy, radius * 2, radius * 2);
};


void Line::update(float timeTook) {
    gradient.x = endPosition.x - position.x;
    gradient.y = endPosition.y - position.y;
    normal = gradient.perpendicular(side);
    normal.norm();
};

void Line::render() {
    float dx = position.x, dy = position.y;
    float dx2 = endPosition.x, dy2 = endPosition.y;
    
    Projection::world_to_screen(dx, dy);
    Projection::world_to_screen(dx2, dy2);
    
    Draw::line(dx, dy, dx2, dy2);
};

class Pendulum : public WorldObject {
    public:
       float angle;
       float angularAcceleration;
       float angularVelocity;
       
       float length;
       float damping;
       
       Ball *knob;
       Vec2f knobPosition, drawnKnobPosition;
       Pendulum(float length, Ball *knob) : WorldObject(knob->mass) {
           this->angle = M_PI;
           this->angularVelocity = 0;
           this->angularAcceleration = 0;
           this->length = length;
           this->damping = 0.995f;
           
           this->knob = knob;
           
           this->name = "pendulum";
           this->knobPosition = position;
       }
       void add(std::vector<WorldObject*> &vec) {
           knob->place(knobPosition.x, knobPosition.y);
           knob->index = vec.size();
           vec.push_back(knob);
       }
       void place(Vec2f pos) {
           float x = pos.x + cos(angle) * length;
           float y = pos.y + sin(angle) * length;
           
           this->knob->place(x, y);
           this->knobPosition.x = x;
           this->knobPosition.y = y;
       }
       void apply(Vec2f vel) {
           Vec2f p = vel;
           Vec2f gradient = { knobPosition.x - position.x, knobPosition.y - position.y };
           float cr = gradient.cross_prod(p);
           float len = gradient.len2();
            
           angularVelocity = (cr / len);
       }
       void update(float timeTook) override;
       void render() override;
};
void Pendulum::update(float timeTook) {
     knob->update(timeTook);
     knobPosition = knob->position;
     
     float l = length;
     if (knob->colliding != nullptr) {
         apply(knob->colliding->vel);
         knob->colliding = nullptr;
     } else {  
         angularAcceleration = (Vars::gravity.y / l) * sin(angle);
         angularVelocity += angularAcceleration;
         angularVelocity *= damping;
         angle += angularVelocity * timeTook;
     }
       
     float a = angle - Utils::radians(90);
     float px = position.x + cos(a) * l;
     float py = position.y + sin(a) * l;
     
     Vec2f gradient = { position.x - knobPosition.x, position.y - knobPosition.y };
     Vec2f nor = gradient.perpendicular(-1);
     nor.norm();
     nor.multiply(knob->mass * length * sin(angle));
     
     knob->vel = nor;
     knob->position.x = px;
     knob->position.y = py; 
       
     drawnKnobPosition = knob->position;
};
void Pendulum::render() {
     float dx = position.x, dy = position.y;
     float ox = drawnKnobPosition.x, oy = drawnKnobPosition.y;
     //float mx = knob->vel.x + ox, my = knob->vel.y + oy;
     
     Projection::world_to_screen(dx, dy);
     Projection::world_to_screen(ox, oy);
     //Projection::world_to_screen(mx, my);
     
     Draw::line(dx, dy, ox, oy);
     // Layering issue fix
     knob->render();
     // Debug drawing
     //Draw::line(ox, oy, mx, my);
};

class Game
{
   public:
      const char *displayName = "";
      virtual ~Game() {};
      virtual void init() {};
      virtual void load() {};
    
      virtual void handle_event(SDL_Event ev) {};

      virtual void update(float timeTook) {};
};

class Aluminium : public Game {
    Ball *player;
    std::vector<WorldObject*> objects;
    public:
       void init() override {
           displayName = "Aluminium";
       } 
       void load() override {
           Assets::get().load(TEXTURES);
           
           add_ball(600, -300, "aluminium-ball", 16, 1.7, true);
           
           add_pendulum(new Ball("aluminium-ball", 16, 10), 1100, -110, 70);
           add_ball(800, -1000, "wooden-ball", 16, 1.0);
            
           add_rectangle("wooden-beam", 0, 0, 10000, 40);
               
           add_rectangle("wooden-plank", 500, -150, 150, 150);
           add_rectangle("wooden-plank", 750, -150, 200, 40, -30);
           
       }
    
       void handle_event(SDL_Event ev) override {
           int cx = 0, cy = 0;
           SDL_GetMouseState(&cx, &cy);
           
           float f = cx > SCREEN_WIDTH / 2 ? 4 : -4;
           player->vel.x += f;
           
           if (player->colliding != nullptr) {
               if (cy < SCREEN_WIDTH / 2) {  
                   player->jump(300, player->colliding);
                   player->colliding = nullptr;
               }
           }
       }
       void update(float timeTook) override { 
           for (auto &obj : objects) {
                obj->update(timeTook);
           }
           Projection::adjust_camera(player->position.x, player->position.y);
           
           // Collision detection
           for (auto &obj : objects) {
                const char *name = obj->name;
                if (name == "ball") {
                     Ball *ball = (Ball*) obj;
                     
                     for (auto &other : objects) {
                          if (ball->index != other->index) {
                              if (other->name == "line") {
                                  Line *line = (Line*) other;
                                  CollisionData dat = ball->collision(other);
                                  Vec2f intersection = dat.intersection_point;   
                                  if (dat.collided) {
                                      ball->colliding = line;
                                      
                                      // Calculate distance
                                      float dst = ball->position.dst(intersection);
                                      float d = ball->radius - dst;
                    
                                      ball->moveX(-d * (intersection.x - ball->position.x) / dst);
                                      ball->moveY(-d * (intersection.y - ball->position.y) / dst);
                    
                                      // Elastic collision
                                      Vec2f nor = line->normal;
                    
                                      float dotP = nor.dot_prod(ball->vel);
                                      float j = 2 * dotP / (ball->mass + line->mass);
                    
                                      ball->vel.x = ball->vel.x - j * nor.x * line->mass;
                                      ball->vel.y = ball->vel.y - j * nor.y * line->mass;
                                  }
                              }
                              if (other->name == "rectangle") {
                                  Rectangle *r = (Rectangle*) other;
                                  CollisionData dat = ball->collision(r);
                                  if (dat.collided) {
                                      ball->colliding = r;
                                      Vec2f p = dat.intersection_point;
                                      Vec2f m = r->position;
                                      
                                      // Retransform the intersection point
                                      m.add(r->width / 2, r->height / 2);
                                      p.subtract(m);
                                      p.rotate(r->angle);
                                      p.add(m.x, m.y);
                                      
                                      // Static collision
                                      float dst = ball->position.dst(p);
                                      float d = ball->radius - dst;
                    
                                      ball->moveX(-d * (p.x - ball->position.x) / dst);
                                      ball->moveY(-d * (p.y - ball->position.y) / dst);
                    
                                      // Elastic collision
                                      Vec2f nor = p;
                                      nor.subtract(ball->position);
                                      nor.norm();
                                      
                                      float dotP = nor.dot_prod(ball->vel);
                                      float j = 2 * dotP / (ball->mass + r->mass);
                    
                                      ball->vel.x = ball->vel.x - j * nor.x * r->mass;
                                      ball->vel.y = ball->vel.y - j * nor.y * r->mass;
                                  }
                              }
                              if (other->name == "ball") {
                                  Ball *ball2 = (Ball*) other;
                                  CollisionData dat = ball->collision(ball2);
                                  if (dat.collided) {
                                      ball->colliding = ball2;
                                      
                                      // Calculate distance again
                                      float dst = ball->position.dst(ball2->position);
                                      float d = dst - ball->radius - ball2->radius;
                                      d *= 0.5;
                               
                                      float bx1 = ball->position.x;
                                      float by1 = ball->position.y;
                                
                                      float bx2 = ball2->position.x;
                                      float by2 = ball2->position.y;
                                
                                      ball->moveX(-d * (bx1 - bx2) / dst);
                                      ball->moveY(-d * (by1 - by2) / dst);
                               
                                      ball2->moveX(d * (bx1 - bx2) / dst);
                                      ball2->moveY(d * (by1 - by2) / dst);
                               
                                      // Elastic collision
                                      Vec2f gradient = { ball2->position.x - ball->position.x, ball2->position.y - ball->position.y };
                                      Vec2f gradientVelocity = { ball->vel.x - ball2->vel.x, ball->vel.y - ball2->vel.y };  
                                      Vec2f nor = gradient;
                                      nor.norm();
                               
                                      float dotP = nor.dot_prod(gradientVelocity);
                                      float j = 2 * dotP / (ball->mass + ball2->mass);
                    
                                      ball->vel.x = ball->vel.x - j * nor.x * ball2->mass;
                                      ball->vel.y = ball->vel.y - j * nor.y * ball2->mass;
                               
                                      ball2->vel.x = ball2->vel.x + j * nor.x * ball->mass;
                                      ball2->vel.y = ball2->vel.y + j * nor.y * ball->mass;
                                  }
                              }
                          }
                     }
                }
           }
           // Rendering
           Draw::color(0.1, 0.1, 0.85);
           Draw::rect_fill_uncentered(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
           Draw::color(1.0, 1.0, 1.0);
           
           for (auto &obj : objects) {
                obj->render();
           }
       }
       void add_line(float x1, float y1, float x2, float y2) {
           add_line(x1, y1, x2, y2, 0);
       }
       void add_line(float x1, float y1, float x2, float y2, int pointing) {
           Line *line = new Line({x1, y1}, {x2, y2});
           line->side = pointing;
           
           line->index = objects.size();
           objects.push_back(line);
       }
       
       void add_ball(float x, float y, const char *spriteName, float radius, float mass) {
           add_ball(x, y, spriteName, radius, mass, false);
       }
       void add_ball(float x, float y, const char *spriteName, float radius, float mass, bool isPlayer) {
           Ball *ball = new Ball(spriteName, radius, mass);
           ball->place(x, y);
           if (isPlayer) {
               player = ball;
           }
           ball->index = objects.size();
           objects.push_back(ball);
       }
       void add_pendulum(Ball *ball, float x, float y, float length) {
           Pendulum *p = new Pendulum(length, ball);
           p->position.x = x;
           p->position.y = y;
           p->place({x, y});
           p->add(objects);
           
           p->index = objects.size();
           objects.push_back(p);
       }
       void add_rectangle(const char *spriteName, float centerX, float centerY, float width, float height, float angle) {
           Rectangle *r = new Rectangle(spriteName, width, height, angle);
           r->place(centerX, centerY);
           
           r->index = objects.size();
           objects.push_back(r);
       }
       void add_rectangle(const char *spriteName, float centerX, float centerY, float width, float height) {
           add_rectangle(spriteName, centerX, centerY, width, height, 0);
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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetVSync(renderer, 1);

    game.load();
    
    float then = 0.0f, delta = 0.0f;
    float now = SDL_GetPerformanceCounter();
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
        then = now;
        now = SDL_GetPerformanceCounter();
        delta = (now - then) * 1 / SDL_GetPerformanceFrequency();
        
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
