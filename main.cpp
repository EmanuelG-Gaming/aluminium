#include <map>
#include <vector>
#include <list>
#include <cmath>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

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

static TTF_Font *get_font(int scaling)
{
    if (TTF_Init() == -1)
	{
		fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
		return NULL;
	}
	TTF_Font *font = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf", scaling);
	if (font == NULL)
	{
		fprintf(stderr, "TTF_OpenFont Error: %s\n", TTF_GetError());
		return NULL;
	}
	return font;
}

static SDL_Texture *load_text(SDL_Color color, const char *text, int scaling)
{
	TTF_Font *font = get_font(scaling);
	SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	if (texture == NULL)
	{
		fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
		return NULL;
	}
	return texture;
}
static SDL_Texture *load_text(SDL_Color color, const char *text)
{
	return load_text(color, text, (int) 25.6);
}
static SDL_Texture *load_text(const char *text)
{
	return load_text({255, 255, 255}, text, (int) 25.6);
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
namespace Interpolation {
    class Interp {
       public:
           Interp(std::function<float(float)> interp) {
               this->interpolation = interp;
           }
           Interp() {}
           
           float at(float alpha) {
               float interpValue = this->interpolation(alpha);
               return interpValue;
           }
       protected:
           std::function<float(float)> interpolation;
    };
    Interp 
    *linear = new Interp([](float a) -> float { return a; }),
    *smooth = new Interp([](float a) -> float { return a * a * (3 - 2 * a); });
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
namespace BoundingBox {
    SDL_Rect &find_bounding_box(SDL_Rect &dest, float angle) {
        Vec2f center = { float(dest.x) + (float(dest.w) / 2), float(dest.y) + (float(dest.h) / 2) };
        
        Vec2f v1 = { dest.x - center.x, dest.y - center.y };
        Vec2f v2 = { dest.x + dest.w - center.x, dest.y - center.y };
        Vec2f v3 = { dest.x - center.x, dest.y + dest.h - center.y };
        Vec2f v4 = { dest.x + dest.w - center.x, dest.y + dest.h - center.y };
        
        // Rotate
        float a = Utils::radians(angle);
        Vec2f r1 = v1.rotate(a);
        Vec2f r2 = v2.rotate(a);
        Vec2f r3 = v3.rotate(a);
        Vec2f r4 = v4.rotate(a);
        
        Vec2f min = { std::min({r1.x, r2.x, r3.x, r4.x}), std::min({r1.y, r2.y, r3.y, r4.y}) };
        Vec2f max = { std::max({r1.x, r2.x, r3.x, r4.x}), std::max({r1.y, r2.y, r3.y, r4.y}) };
        
        SDL_Rect result;
        result.x = min.x + center.x;
        result.y = min.y + center.y;
        result.w = max.x - min.x;
        result.h = max.y - min.y;
        
        return result;
    };
};

enum LoadStages{
    TEXTURES,
    LEVELS
};

struct CollisionData {
    // This doesn't have to be exactly inside the object to collide with
    Vec2f intersection_point;
    bool collided;
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
            
            reset();
        }
        virtual void place(float x, float y) {
             position.x = x;
             position.y = y;
        }
        void moveX(float x) {
             position.x += x;
        }
        void moveY(float y) {
             position.y += y;
        }
        // Useful for things like buoyant force
        void apply_force(Vec2f f) {
             vel.x += (f.x / mass);
             vel.y += (f.y / mass);
        }
        virtual void reset() {
             vel.set_zero();
             acceleration.set_zero();
        } 
        virtual CollisionData collision(WorldObject *object) {}
        virtual void do_collision(WorldObject *object) {}
        
        virtual void update(float timeTook) {}
        virtual void render() {}
};

namespace Collisions {
    void solve_elastic(Vec2f &pos1, Vec2f &pos2, Vec2f &vel1, Vec2f &vel2, Vec2f &normal, float mass1, float mass2, bool otherUnmovable) {
        Vec2f gradient = { pos2.x - pos1.x, pos2.y - pos1.y };
        Vec2f gradientVelocity = { vel1.x - vel2.x, vel1.y - vel2.y };
                               
        float dotP = normal.dot_prod(gradientVelocity);
        float j = 2 * dotP / (mass1 + mass2);
                    
        vel1.x = vel1.x - j * normal.x * mass2;
        vel1.y = vel1.y - j * normal.y * mass2;
        
        if (!otherUnmovable) {                   
            vel2.x = vel2.x + j * normal.x * mass1;
            vel2.y = vel2.y + j * normal.y * mass1;
        }
    }
    void solve_elastic(WorldObject *first, WorldObject *second, Vec2f &normal, bool otherUnmovable) {
        solve_elastic(first->position, second->position, first->vel, second->vel, normal, first->mass, second->mass, otherUnmovable);
    }
};

class Level {
      public:
          Vec2f playerStartPos;
          int index = 0;
          const char *name;
          Level(const char *name);
          void add(WorldObject *obj, float x, float y);
          void add(WorldObject *obj, float x, float y, bool load);
          void set_start_position(float x, float y) {
              playerStartPos.x = x;
              playerStartPos.y = y;
          }
          
          void set_objects(std::vector<WorldObject*> &other) {
              objects = other;
          }
          std::vector<WorldObject*> &get_objects() {
              return objects;
          }
          std::vector<WorldObject*> &get_loaded() {
              return loaded;
          }
      protected:
          std::vector<WorldObject*> objects;
          std::vector<WorldObject*> loaded;
};
void Level::add(WorldObject *obj, float x, float y, bool load) {
      obj->place(x, y);
      if (load) {
          obj->index = loaded.size();
      
          loaded.push_back(obj);
      } else {
          obj->index = objects.size();
      
          objects.push_back(obj);
      }
};

void Level::add(WorldObject *obj, float x, float y) {
      this->add(obj, x, y, true);
};

class Assets {
    std::map<const char*, SDL_Texture*> textures;
    Level *currentLevel = nullptr;
    public:
        std::map<const char*, Level*> levels;
        
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
        Level *find_level(const char *location) {
            return levels[location];
        }
        void add_level(Level *l, const char *location) {
            levels[location] = l;
        }
        
        void load(LoadStages stage);
    private:
        Assets() {}
        ~Assets() {}
    public:
        Assets(Assets const&) = delete;
        void operator = (Assets const&) = delete;    
};

Level::Level(const char *name) {
    this->objects.empty(); 
    this->playerStartPos.set_zero();
    this->name = name;
    this->index = Assets::get().levels.size();
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
    void rect_uncentered(int x, int y, int w, int h)
    {
        SDL_Rect dest = { x, y, w, h };
        SDL_RenderDrawRect(renderer, &dest);
    } 
    void rect(int x, int y, int w, int h)
    {
        SDL_Rect dest = { x - w / 2, y - h / 2, w, h };
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) 
            SDL_RenderDrawRect(renderer, &dest);
    }
    void bounds(int x, int y, int w, int h, int thickness) {
        rect_fill_uncentered(x, y, w, thickness);
        rect_fill_uncentered(x, y, thickness, h);
        rect_fill_uncentered(x, y + h - thickness, w, thickness);
        rect_fill_uncentered(x + w - thickness, y, thickness, h);
    }
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
        SDL_Rect b = BoundingBox::find_bounding_box(cRect, angle);
        SDL_Rect v = Utils::get_viewport_rect();
        if (Utils::rectangle_collide(&b, &v)) {
            SDL_RenderCopyEx(renderer, tex, NULL, &cRect, angle, NULL, SDL_FLIP_NONE);
        }
        // Debug drawing
        //rect_uncentered(b.x, b.y, b.w, b.h);
    }
    void text(SDL_Texture *tex, int x, int y, float scaling) {
        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
           
        SDL_Rect d;
        d.w = (int) tw * scaling;
    	d.h = (int) th * scaling;
        d.x = x - d.w / 2;
    	d.y = y - d.h / 2;
       	
        SDL_RenderCopy(renderer, tex, NULL, &d);
    }
    void text(SDL_Texture *tex, int x, int y) {
        text(tex, x, y, 1.0f);
    }
    void mix_color(SDL_Texture *tex, SDL_Color color) {
        SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
    }
    void alpha(SDL_Texture *tex, float alpha) {
        SDL_SetTextureAlphaMod(tex, alpha);
    }
    void blend(SDL_Texture *tex, SDL_BlendMode blend) {
        SDL_SetTextureBlendMode(tex, blend);
    }
    void line(int x1, int y1, int x2, int y2)
    {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
};

class Action {
    public:
       bool started;
       bool completed;
       // Whether or not this action hinders player control, unless done
       bool allowPlayerControl;
       // Whether or not the camera can move to the player's position
       bool canMoveCamera;
       Action() {
           this->started = false;
           this->completed = false;
           this->allowPlayerControl = false;
           this->canMoveCamera = true;
       }
       // Called when the action is first started
       virtual void run() {}
       // Called when the action is happening
       virtual void update(float timeTook) {}
       // Optional rendering
       virtual void render() {}
       virtual void handle_event(SDL_Event ev) {}
       // Called when the action is finished
       virtual void finish() {} 
};

// An action that takes a certain amount of time to complete
class TimedAction : public Action {
    public:
       TimedAction(float timeToComplete) : Action() {
           this->timeToComplete = timeToComplete;
       };
       virtual void run() override {
           this->time = 0;
       }
       void update(float timeTook) override;
       float in() { return time / timeToComplete; }
       float out() { return 1 - in(); }
    protected:
       float timeToComplete;
       float time;
};
void TimedAction::update(float timeTook) {
    time += timeTook;
    
    if (time >= timeToComplete) {
        completed = true;
    }
};

class LevelBeginAction : public TimedAction {
    Level *beginLevel;
    SDL_Texture *text;
    public:
       LevelBeginAction(const char *levelName) : TimedAction(4.0f) {
           SDL_Color white = { 255, 255, 255 };
           
           this->beginLevel = Assets::get().find_level(levelName);
           this->text = load_text(white, this->beginLevel->name);
           
           this->allowPlayerControl = true;
       };
       void render() override;
};

void LevelBeginAction::render() {
    float alpha = Interpolation::smooth->at(out());
    
    Draw::alpha(this->text, alpha * 255);
    Draw::text(this->text, SCREEN_WIDTH / 2, 70);
};

class CameraMoveAction : public TimedAction {
    public:
       CameraMoveAction(Vec2f toPosition, float duration) : TimedAction(duration) {
           this->toPosition = toPosition;
           this->canMoveCamera = false;
           this->turnsBack = false;
       }
       CameraMoveAction(float duration) : TimedAction(duration) {
           this->canMoveCamera = false;
           this->turnsBack = true;
       }
       void run() override;
       void update(float timeTook) override;
    protected:
       bool turnsBack;
       Vec2f fromPosition;
       Vec2f toPosition;
};

class ActionProcessor {
    std::list<Action*> actions;
    public:
       static ActionProcessor &get()
       {
           static ActionProcessor ins;
           return ins;
       }
       void update(float timeTook);
       void render();
       
       void add(Action *act);
       void subtract();
       Action *front();
       
       bool started();
    private:
        ActionProcessor() {}
        ~ActionProcessor() {}
    public:
        ActionProcessor(ActionProcessor const&) = delete;
        void operator = (ActionProcessor const&) = delete;    
};
void ActionProcessor::add(Action *act) {
    actions.push_back(act);
};
Action *ActionProcessor::front() {
    if (started())
        return actions.front();
    else return new Action();
};
void ActionProcessor::subtract() {
    if (started()) {
        delete front();
        actions.pop_front();
    }
};
void ActionProcessor::update(float timeTook) {
    Action *a = front();
    if (started()) {
        if (!a->completed) {
           if (!a->started) { a->run(); a->started = true; }
           else a->update(timeTook);
        } else {
           a->finish();
           subtract();
        }
    }
};
void ActionProcessor::render() {
    Action *a = front();
    if (started()) {
        if (!a->completed) {
            a->render();
        }
    }
};
bool ActionProcessor::started() {
    return actions.size() > 0;
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
        float circumference() {
            return 2 * M_PI * radius;
        }
        float area() {
            return M_PI * radius * radius;
        }  
        void jump(float force, WorldObject *o);
        void update(float timeTook) override;
        void render() override;
        
        CollisionData collision(WorldObject *object) override;
        void do_collision(WorldObject *object) override;
};
namespace Vars {
    Ball *player;
    Level *currentLevel = nullptr;
    // Whether or not the player can move before the cursor hovers over an UI element
    bool focused;
    Vec2f lastPlacePosition;
    Vec2f gravity = { 0.0f, 9.8f };
    // Measured in radians
    float gravity_angle() {
        return atan2(-gravity.y, gravity.x);
    }
    void load_level(const char *levelName, float playerX, float playerY, bool hasText) {
        Level *toLevel = Assets::get().find_level(levelName);
        if (toLevel != nullptr) {
            lastPlacePosition.x = playerX;
            lastPlacePosition.y = playerY;
            currentLevel = toLevel;
            currentLevel->get_objects().clear();
            currentLevel->set_objects(toLevel->get_loaded());
            
            for (auto &obj : currentLevel->get_objects()) {
                 obj->reset();
            }
            player->place(playerX, playerY);
            player->index = currentLevel->get_objects().size();
            
            currentLevel->get_objects().push_back(player);
            if (hasText) {
                ActionProcessor::get().add(new LevelBeginAction(levelName));  
            }
        } 
    }
    void load_level(const char *levelName, bool hasText) {
        Level *toLevel = Assets::get().find_level(levelName);
        if (toLevel != nullptr) {
            load_level(levelName, toLevel->playerStartPos.x, toLevel->playerStartPos.y, hasText);
        }
    }
};
void CameraMoveAction::run() {
    this->time = 0.0f;
    if (turnsBack) {
        toPosition = Vars::player->position;
    }
    fromPosition.x = Projection::cameraX;
    fromPosition.y = Projection::cameraY;
};
void CameraMoveAction::update(float timeTook) {
    time += timeTook;
    
    float alpha = Interpolation::smooth->at(in());
    Vec2f interp = fromPosition.interpolate(toPosition, alpha);
    
    // Interpolate the camera's position
    Projection::adjust_camera(interp.x, interp.y);
    
    if (time >= timeToComplete) {
        completed = true;
    }
};

class LevelCompleteAction : public TimedAction {
    const char *toLevelName;
    public:
       LevelCompleteAction(const char *toLevelName) : TimedAction(2.5f) {
           this->toLevelName = toLevelName;
       }
       void finish() override; 
};

void LevelCompleteAction::finish() {
    // Load level
    Vars::load_level(toLevelName, true);
    printf("loaded ");
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
        Rectangle(float width, float height, float angle) : WorldObject(4.0f) {
            this->width = width;
            this->height = height;
            this->angle = Utils::radians(angle);
          
            this->name = "rectangle";
        }
        virtual void render() override;
    protected:
        SDL_Texture *rectangleTexture;
};
void Rectangle::render() {
     float ox = position.x, oy = position.y;
     Projection::world_to_screen(ox, oy);
     Draw::rotated_texture(rectangleTexture, ox, oy, width, height, Utils::degrees(angle));
};

class Trigger : public Rectangle {
    public:
        std::function<void(WorldObject *other)> trigger;
        // Does nothing on touch
        Trigger(float width, float height, float angle) : Rectangle(width, height, angle) {
            this->triggered = false;
            
            this->name = "trigger";
        }
        Trigger(float width, float height, float angle, std::function<void(WorldObject *other)> trigger) : Rectangle(width, height, angle) {
            this->triggered = false;
            this->name = "trigger";
            
            this->trigger = trigger;
        }
        void reset() override;
        void collide(WorldObject *o);
        void render() override {}
    protected:
        bool triggered;
};
void Trigger::reset() {
    triggered = false;
};
void Trigger::collide(WorldObject *o) {
    if (!triggered) {
        trigger(o);
        triggered = true;
    }
};

class Liquid : public Rectangle {
    public:
        SDL_Color color;
        int alpha;
        float density;
        Liquid(SDL_Color color, float density, float width, float height, float angle) : Rectangle("white-texture", width, height, angle) {
            this->density = density;
            this->color = color;
            this->alpha = 180;
            Draw::mix_color(this->rectangleTexture, color);
            Draw::alpha(this->rectangleTexture, alpha);
     
            this->name = "liquid";
        }
        float get_time_unit() { return timeUnit; }
        void update(float timeTook) override;
        void render() override;
    protected:
        float timeUnit;
};
void Liquid::update(float timeTook) {
    this->timeUnit = timeTook;
};
void Liquid::render() {
     float ox = position.x, oy = position.y;
     Projection::world_to_screen(ox, oy);
     
     
     Draw::rotated_texture(rectangleTexture, ox, oy, width, height, Utils::degrees(angle));
};

class Flag : public Trigger {
    const char *toLevelName;
    public:
        Flag(const char *toLevelName, float angle) : Trigger(30, 60, angle) {
             this->rectangleTexture = Assets::get().find_texture("flag");
             this->toLevelName = toLevelName;
             
             trigger = [&](WorldObject *o) { 
                 ActionProcessor::get().add(new LevelCompleteAction(this->toLevelName));
             };
        }
        
        void render() override;
};
void Flag::render() {
     float ox = position.x, oy = position.y;
     Projection::world_to_screen(ox, oy);
     
     Draw::rotated_texture(rectangleTexture, ox, oy, width, height, Utils::degrees(angle));
};
 

void Ball::update(float timeTook) {
    // Ball kinematics
    acceleration.x = -vel.x * resistance + Vars::gravity.x * 60;
    acceleration.y = -vel.y * resistance + Vars::gravity.y * 60;
    
    vel.x += acceleration.x * timeTook;
    vel.y += acceleration.y * timeTook;
    
    position.x += vel.x * timeTook;
    position.y += vel.y * timeTook;
    
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
     // Colliding with a rectangle/trigger/liquid
     if (((oName == "rectangle") || (oName == "trigger") || (oName == "liquid"))) {
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
void Ball::do_collision(WorldObject *obj) {
     if (obj->name == "line") {
         Line *line = (Line*) obj;
         CollisionData dat = collision(line);
         if (dat.collided) {
              this->colliding = line;

              Vec2f intersection = dat.intersection_point;
              float dst = position.dst(intersection);
              // Don't divide by zero
              if (dst > 0) {
                  float d = radius - dst;
                    
                  moveX(-d * (intersection.x - position.x) / dst);
                  moveY(-d * (intersection.y - position.y) / dst);
                  
                  Collisions::solve_elastic(this, line, line->normal, true);
              }
          }
     }
     
     if (obj->name == "rectangle") {
         Rectangle *r = (Rectangle*) obj;
         CollisionData dat = this->collision(r);
         
         if (dat.collided) {
             this->colliding = r;
             Vec2f p = dat.intersection_point;
             Vec2f m = r->position;
                                      
             // Retransform the intersection point
             m.add(r->width / 2, r->height / 2);
             p.subtract(m);
             p.rotate(r->angle);
             p.add(m.x, m.y);
                                      
             float dst = this->position.dst(p);
             // Don't divide by zero
             if (dst > 0) {
                 float d = this->radius - dst;
                    
                 moveX(-d * (p.x - position.x) / dst);
                 moveY(-d * (p.y - position.y) / dst);
                 
                 Vec2f nor = p;
                 nor.subtract(this->position);
                 nor.norm();                
                 Collisions::solve_elastic(this, r, nor, true);
             } 
         }
     } 
        
     if (obj->name == "ball") {
         Ball *ball2 = (Ball*) obj;
         CollisionData dat = this->collision(ball2);
         if (dat.collided) {
             this->colliding = ball2;
             float dst = this->position.dst(ball2->position);
             
             // Don't divide by zero
             if (dst > 0) {
                 float d = dst - this->radius - ball2->radius;
                 d *= 0.5;
                               
                 float bx1 = this->position.x;
                 float by1 = this->position.y;          
                 float bx2 = ball2->position.x;
                 float by2 = ball2->position.y;
                                
                 moveX(-d * (bx1 - bx2) / dst);
                 moveY(-d * (by1 - by2) / dst);
                               
                 ball2->moveX(d * (bx1 - bx2) / dst);
                 ball2->moveY(d * (by1 - by2) / dst);
                 
                 Vec2f normal = { ball2->position.x - this->position.x, ball2->position.y - this->position.y }; 
                 normal.norm();                   
                 Collisions::solve_elastic(this, ball2, normal, false);
            }
        }
    }
    
    if (obj->name == "trigger") {
        Trigger *trig = (Trigger*) obj;
        CollisionData dat = this->collision(trig);
        if (dat.collided) {
            if (this == Vars::player) {
                trig->collide(this);
            }
        }
    }
    if (obj->name == "liquid") {
        Liquid *l = (Liquid*) obj;
        CollisionData dat = this->collision(l);
        if (dat.collided) {
            // Approximate buoyant force
            float f = l->density * this->area() * -Vars::gravity.y;
                                      
            Vec2f m = { 0, 1 };
            m.multiply(f * l->get_time_unit());
                                      
            this->apply_force(m);
        }
    }
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

class BalloonVertice {
     public:
        Vec2f position;
        Vec2f vel;
        Vec2f gravityAcceleration;
        Vec2f force;
        
        float mass;
        float containedArea;
        float resistance = 0.7f;
        WorldObject *source;
        
        CollisionData collide(WorldObject *obj);
        
        void do_collision(WorldObject *obj);
        void do_vertice_collision(BalloonVertice *other);
};

CollisionData BalloonVertice::collide(WorldObject *obj) {
     CollisionData data;
     
     const char *oName = obj->name;
     if (oName == "ball") {
         Ball *ball = (Ball*) obj;
         
         Vec2f gradient = { ball->position.x - this->position.x, ball->position.y - this->position.y };
         float dst2 = gradient.len2();
         if (dst2 > 0) {
             gradient.norm();
             gradient.multiply(ball->radius);
         
             Vec2f pos = gradient;
             pos.x += ball->position.x;
             pos.y += ball->position.y;
         
             data.collided = dst2 < (ball->radius * ball->radius + 3 * 3);
             data.intersection_point = pos;
         }
     }
     if ((oName == "rectangle") || (oName == "liquid")) {
          Rectangle *dest = (Rectangle*) obj;
          Vec2f centerRectangle = dest->position;
          centerRectangle.add(dest->width / 2, dest->height / 2);
          
          Vec2f gradient = { position.x - centerRectangle.x, position.y - centerRectangle.y };
          Vec2f r = gradient;
          r.rotate(-dest->angle);
          r.add(centerRectangle.x, centerRectangle.y);
          
          Vec2f intersection;
          
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
          data.collided = m.len2() <= (3 * 3);
          data.intersection_point = intersection;
     }
     return data;
};

void BalloonVertice::do_vertice_collision(BalloonVertice *other) {
     float dst = this->position.dst(other->position);
     
     // Don't divide by zero
     if ((dst <= (9 + 9)) && (dst > 0)) {  
        float d = dst - 9 - 9;
        d *= 0.5;
                               
        float bx1 = this->position.x;
        float by1 = this->position.y;          
        float bx2 = other->position.x;
        float by2 = other->position.y;
                                
        this->position.x += -d * (bx1 - bx2) / dst;
        this->position.y += -d * (by1 - by2) / dst;
                               
        other->position.x += d * (bx1 - bx2) / dst;
        other->position.y += d * (by1 - by2) / dst;
                 
        Vec2f normal = { other->position.x - this->position.x, other->position.y - this->position.y }; 
        normal.norm();                   
        Collisions::solve_elastic(this->position, other->position, this->vel, other->vel, normal, this->mass, other->mass, false);
            
    }
};

class Spring {
     public:
        BalloonVertice *p1, *p2;
        float damping, stiffness, rest_length;
        
        // Interior forces
        Vec2f f1, f2;
        // Normal
        Vec2f normal;
        
        void update() {
             Vec2f springs = { p2->position.x - p1->position.x, p2->position.y - p1->position.y };
             float dst = springs.len();
             if (dst > 0) {
                 float vx = (p2->vel.x - p1->vel.x) * (p2->position.x - p1->position.x);
                 float vy = (p2->vel.y - p1->vel.y) * (p2->position.y - p1->position.y);
                 float f = (dst - rest_length) * stiffness + (vx + vy) * damping / dst;
                 
                 springs.norm();
                 Vec2f nor = springs.perpendicular(-1);
                 springs.multiply(f);
              
                 f1.x = springs.x;
                 f1.y = springs.y;
              
                 f2.x = -springs.x;
                 f2.y = -springs.y;
              
                 normal = nor; 
             }
         }
         void apply() {
              p1->force.x += f1.x;
              p1->force.y += f1.y;
         
              p2->force.x += f2.x;
              p2->force.y += f2.y;
         }
};
     
class Balloon : public WorldObject {
     public:
        float damping;
        float stiffness;
        float rest_length;
        float spring_constant = 30000;
        
        float radius;
        int sides;
        SDL_Color color;
        BalloonVertice *collidingWithBall = nullptr;
        Balloon(float pointMass) : WorldObject(pointMass) {
            this->sides = 12;
            this->radius = 10.0f;
            this->damping = 10;
            this->stiffness = 100;
            
            this->name = "balloon";
            this->color = { 255, 255, 255 };
        }
        Balloon(int sides, float radius, float damping, float stiffness, float pointMass) : WorldObject(pointMass) {
            this->sides = sides;
            this->radius = radius;
            this->damping = damping;
            this->stiffness = stiffness;
            
            this->name = "balloon";
            this->color = { 255, 255, 255 };
        }
        void generate_points(int side) {
            // Generate a regular polygon
            float a = 0;
            for (int i = 0; i < side; i++) {
                BalloonVertice *vert = new BalloonVertice();
                
                a += (M_PI * 2 / side);
                float px = cos(a) * radius;
                float py = sin(a) * radius;
                Vec2f p = { px, py };
                vert->position = p;
                vert->mass = this->mass;
                vert->containedArea = this->area();
                
                vertices.push_back(vert);
            }
            add_springs();
            this->position = centroid();
        }
        void generate_points() {
            this->generate_points(this->sides);
        }
        void generate_points(std::function<void()> consumer) {
            consumer();
            add_springs();
            this->position = centroid();
        }
        void add_springs() {
            for (int i = 1; i < this->sides; i++) {
                 //for (int j = i + 1; j < this->sides; j++) {
                      Spring *s = new Spring();
                      s->p1 = vertices.at(i - 1);
                      s->p2 = vertices.at(i);
                    
                      s->damping = damping;
                      s->stiffness = stiffness;
                      s->rest_length = s->p1->position.dst(s->p2->position);
                    
                      springs.push_back(s);
                 
            }
            Spring *s = new Spring();
            s->p1 = vertices.at(vertices.size() - 1);
            s->p2 = vertices.at(0);
                    
            s->damping = damping;
            s->stiffness = stiffness;
            s->rest_length = s->p1->position.dst(s->p2->position);
                    
            springs.push_back(s);
            
            for (int k = 0; k < this->sides; k++)
                 vertices.at(k)->source = this;
        }
        void move_points(Vec2f to) {
            for (auto &vert : vertices) {
                vert->position.x += to.x;
                vert->position.y += to.y;
            }
        }
        void place(float x, float y) override {
             position.x = x;
             position.y = y;
             move_points({x, y});
        }
        
        void apply_force_point(BalloonVertice *vertice, Vec2f f) {
             vertice->force.x += f.x;
             vertice->force.y += f.y;
        }
        Vec2f centroid() {
            Vec2f result;
            for (auto &vertice : vertices) {
                Vec2f vert = vertice->position;
                
                result.x += vert.x / this->sides;
                result.y += vert.y / this->sides;
            }
            
            return result;
        }
        Vec2f centroid_velocity() {
            Vec2f result;
            for (auto &vertice : vertices) {
                Vec2f vert = vertice->vel;
                
                result.x += vert.x / this->sides;
                result.y += vert.y / this->sides;
            }
            
            return result;
        }
        // Area inside the balloon
        float area() {
            float a = 0.0f;
            for (auto &s : springs) {
                Vec2f gradient = { s->p2->position.x - s->p1->position.x, s->p2->position.y - s->p1->position.y };
                float len = gradient.len();
                
                a += 0.5 * fabs(s->p2->position.x - s->p1->position.x) * fabs(s->normal.x) * len;
            }
            return a;
        }
        float line_len() {
            float angle = 2 * M_PI / this->sides;
            float side = 2 * radius * tan(angle / 2);
            
            return side;
        }
        std::vector<BalloonVertice*> &get_vertices() { return vertices; }
        
        void update(float timeTook) override;
        void render() override;
     protected:
        std::vector<BalloonVertice*> vertices;
        std::vector<Spring*> springs;
        
};

void Balloon::update(float timeTook) {
     // Calculate centroid
     this->position = centroid();
     this->vel = centroid_velocity();
     
     for (auto &vertice : vertices) {
          vertice->force.set_zero();
          vertice->gravityAcceleration.x = -vertice->vel.x * vertice->resistance + Vars::gravity.x * 60;
          vertice->gravityAcceleration.y = -vertice->vel.y * vertice->resistance + Vars::gravity.y * 60;
    
          vertice->force.x += vertice->gravityAcceleration.x * vertice->mass;
          vertice->force.y += vertice->gravityAcceleration.y * vertice->mass;
          vertice->containedArea = this->area();
     }
     
     // Pressure force
     for (auto &s : springs) {
          Vec2f gradient = { s->p2->position.x - s->p1->position.x, s->p2->position.y - s->p1->position.y };
          float len = gradient.len();
          if (len > 0 && area() > 0) {
              float force = spring_constant * len * (1.0f / area());
              
              Vec2f perpendicular = s->normal;
              perpendicular.multiply(force);
              
              s->p1->force.x += perpendicular.x;
              s->p1->force.y += perpendicular.y;
              
              s->p2->force.x += perpendicular.x;
              s->p2->force.y += perpendicular.y;
          }
     }
     
     for (auto &s : springs) {
         s->update();
     }
     for (auto &s : springs) {
         s->apply();
     }
     
     // Euler integration
     for (auto &vertice : vertices) {
         vertice->vel.x += (vertice->force.x * timeTook) / vertice->mass;
         vertice->vel.y += (vertice->force.y * timeTook) / vertice->mass;
         
         vertice->position.x += vertice->vel.x * timeTook;
         vertice->position.y += vertice->vel.y * timeTook;
     }
     for (auto &vertice : vertices) {
         if (fabs(vertice->vel.len2()) < 0.1f) {
             vertice->vel.set_zero();
         }
     }
};

void Balloon::render() {
     Draw::color((float)color.r / 255, (float)color.g / 255, (float)color.b / 255);
     
     for (int i = 1; i < this->sides; i++) {
         float vx1 = vertices.at(i - 1)->position.x;
         float vy1 = vertices.at(i - 1)->position.y;
         float vx2 = vertices.at(i)->position.x;
         float vy2 = vertices.at(i)->position.y;
         
         Projection::world_to_screen(vx1, vy1);
         Projection::world_to_screen(vx2, vy2);
         
         Draw::line(vx1, vy1, vx2, vy2);  
     }
     float vx3 = vertices.back()->position.x;
     float vy3 = vertices.back()->position.y;
     float vx4 = vertices.front()->position.x;
     float vy4 = vertices.front()->position.y;
     
     Projection::world_to_screen(vx3, vy3);
     Projection::world_to_screen(vx4, vy4);
     
     Draw::line(vx3, vy3, vx4, vy4);
};
void BalloonVertice::do_collision(WorldObject *obj) {
     const char *oName = obj->name;
     if (oName == "ball") {
         Ball *ball2 = (Ball*) obj;
         CollisionData dat = this->collide(ball2);
         if (dat.collided) {
             if (this->source != nullptr) {
                 Balloon *balloon = (Balloon*) this->source;
                 balloon->collidingWithBall = this;
                 ball2->colliding = balloon;
             }
             Vec2f collision = dat.intersection_point;
             float dst = this->position.dst(collision);
              
             // Don't divide by zero
             if (dst > 0) {            
                 float bx1 = this->position.x;
                 float by1 = this->position.y;          
                 float bx2 = ball2->position.x;
                 float by2 = ball2->position.y;
                 Vec2f gradient = { bx1 - collision.x, by1 - collision.y };
                 float d = dst - ball2->radius;
                 d *= 0.5;
                           
                 position.x += d * (bx1 - bx2) / dst;
                 position.y += d * (by1 - by2) / dst;
                               
                 ball2->moveX(-d * (bx1 - bx2) / dst);
                 ball2->moveY(-d * (by1 - by2) / dst);
                 
                 Vec2f normal = { ball2->position.x - this->position.x, ball2->position.y - this->position.y }; 
                 normal.norm();   
                 Collisions::solve_elastic(this->position, ball2->position, this->vel, ball2->vel, normal, this->mass, ball2->mass, false);
            }
        }
     }
     if (obj->name == "rectangle") {
         Rectangle *r = (Rectangle*) obj;
         CollisionData dat = this->collide(r);
         
         if (dat.collided) {
             Vec2f p = dat.intersection_point;
             Vec2f m = r->position;
                                      
             // Retransform the intersection point
             m.add(r->width / 2, r->height / 2);
             p.subtract(m);
             p.rotate(r->angle);
             p.add(m.x, m.y);
                                      
             float dst = this->position.dst(p);
             // Don't divide by zero
             if (dst > 0) {
                 float d = 3 - dst;
                    
                 this->position.x += -d * (p.x - position.x) / dst;
                 this->position.y += -d * (p.y - position.y) / dst;
                 
                 Vec2f nor = p;
                 nor.subtract(this->position);
                 nor.norm();                
                 Collisions::solve_elastic(this->position, r->position, this->vel, r->vel, nor, this->mass, r->mass, true);
             } 
         }
     }
     if (obj->name == "liquid") {
        Liquid *l = (Liquid*) obj;
        CollisionData dat = this->collide(l);
        if (dat.collided) {
            // Approximate buoyant force
            float f = (l->density * this->containedArea * Vars::gravity.y) * l->get_time_unit();
                                      
            Vec2f m = { 0, -1 };
            m.multiply(f);
                                      
            this->vel.x += m.x;
            this->vel.y += m.y;
        }
    }
};

void Ball::jump(float force, WorldObject *o) {
     const char *oName = o->name;
     
     if (oName == "line") {
          Vec2f normal = ((Line*) o)->normal;
          if (normal.dot_prod(Vars::gravity) > 0) {
              vel.x += normal.x * vel.y;
              vel.y += -force + normal.y;
          }
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
          
          // Don't stick to the ceilling
          if (normal.dot_prod(Vars::gravity) > 0) {
              vel.x += normal.x * vel.y;
              vel.y += -force + normal.y;
          }
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
     if (oName == "balloon") {
          Balloon *b = (Balloon*) o;
          float dx = b->collidingWithBall->position.x - position.x;
          float dy = b->collidingWithBall->position.y - position.y;
               
          float angle = atan2(dy, dx);
          float px = cos(angle) * force;
          float py = sin(angle) * force;
               
          vel.x -= px;
          vel.y -= py;
               
          b->collidingWithBall->vel.x += px;
          b->collidingWithBall->vel.y += py;
          b->collidingWithBall = nullptr;
     }
};

class Drawable {
    public:
       virtual void draw(float x, float y, float width, float height, float alpha) {}
};

class TextureDrawable : public Drawable {
    public:
       TextureDrawable(SDL_Texture *texture) : Drawable() {
           this->texture = texture;
           this->tint = { 255, 255, 255 };
       }
       TextureDrawable(SDL_Texture *texture, SDL_Color tint) : Drawable() {
           this->texture = texture;
           this->tint = tint;
       }
       // Draws an uncentered texture
       void draw(float x, float y, float width, float height, float alpha) override;
    protected:
       SDL_Texture *texture;
       SDL_Color tint;
};
void TextureDrawable::draw(float x, float y, float width, float height, float alpha) {
    // Alpha and color modulation
    Draw::mix_color(this->texture, this->tint);
    Draw::alpha(this->texture, 255 * alpha);
    
    Draw::texture_uncentered(texture, x, y, width, height);
    Draw::alpha(this->texture, 255);
};

class Style {
    public:
       virtual void draw(float x, float y, float width, float height, SDL_Color color) {}
};
class LabelStyle : public Style {
    public:
       // Optional
       Drawable *background;
       // Optional tinting
       SDL_Color tint;
       LabelStyle(SDL_Color tint) : Style() {
           this->tint = tint;
           this->background = nullptr;
       }
       LabelStyle(SDL_Color tint, Drawable *back) : Style() {
           this->tint = tint;
           this->background = back;
       }
       LabelStyle() : Style() {
           this->tint = { 255, 255, 255 }; 
           this->background = nullptr;
       }
};

class DefaultButtonStyle : public Style {
    public:
       void draw(float x, float y, float width, float height, SDL_Color color) override {
           Draw::color(color.r, color.g, color.b);
           Draw::bounds(x, y, width, height, 5);
       }
};

namespace Styles {
    LabelStyle *aluminium;
    TextureDrawable *black;
    void load() {
        aluminium = new LabelStyle({0, 0, 0});
        aluminium->background = new TextureDrawable(Assets::get().find_texture("aluminium-ball"));
        black = new TextureDrawable(Assets::get().find_texture("white-ui"), { 0, 0, 0 });
    }
};

template <typename T>
class UIAction {
    public:
       bool started;
       bool completed;
       
       UIAction(T obj) {
           this->started = false;
           this->completed = false;
           this->actObject = obj;
       }
       virtual void start() { this->completed = true; }
       virtual void complete() {}
       virtual void act(float timeTook) {}
    protected:
       T actObject;
};

class UIObject {
    public:
       float width, height;
       Vec2f position;
       Vec2f relativePosition;
       
       SDL_Color color;
       SDL_Color highlightColor;
       float alpha;
       bool touchable;
       UIObject() {
           this->highlighted = false;
           this->touchable = false;
           this->useSetSize = true;
           this->color = { 255, 255, 255 };
           this->highlightColor = { 0, 255, 0 };
           this->alpha = 1.0f;
           
           this->clickListener = [&](){};
           this->hoverListener = [&](){};
           this->updateListener = [&](){};
           this->visible = [&]() -> bool { return true; };
           this->style = nullptr;
       }
       void set_position(Vec2f &to) {
           position.x = to.x;
           position.y = to.y;
       }
       void set_position_centered(Vec2f &center) {
           position.x = center.x - width / 2;
           position.y = center.y - height / 2;
       }
       void act(float timeTook) {
           updateListener();
           
           UIAction<UIObject*> *a = front();
           if (actions.size() > 0) {
               if (!a->completed) {
                   if (!a->started) { a->start(); a->started = true; }
                   else a->act(timeTook);
               } else {
                  a->complete();
                  subtract();
               }
           }
           
           // Do not highlight and focus if the cutscene doesn't allow player control
           if (ActionProcessor::get().started() && !ActionProcessor::get().front()->allowPlayerControl) {
               highlighted = false;
               Vars::focused = false;
           }
       }
       virtual void handle_event(SDL_Event event) {
           int px, py;
           SDL_GetMouseState(&px, &py);
           bool intersecting = hovering(px, py);
           
           if (intersecting) { 
               hoverListener();
               
               if (event.type == SDL_MOUSEMOTION) {
                   highlighted = true;
                   Vars::focused = true;
               }
                  
               if (event.type == SDL_MOUSEBUTTONUP) {
                   clickPosition.x = px;
                   clickPosition.y = py;
                   
                   clickListener();
                   highlighted = false;
                   Vars::focused = false;
               }
           } else {
               highlighted = false;
               Vars::focused = false;
           }
       }
       virtual void draw() {
           if (visible()) {
               SDL_Color c = highlighted ? highlightColor : color;
               style->draw(position.x, position.y, width, height, c);
           }
       }
       UIObject *clicked(std::function<void()> cons) {
           this->clickListener = cons;
           
           return this; 
       }
       UIObject *hovered(std::function<void()> cons) {
           this->hoverListener = cons;
           
           return this; 
       }
       UIObject *update(std::function<void()> cons) {
           this->updateListener = cons;
           
           return this; 
       }
       void add(UIAction<UIObject*> *action) {
           actions.push_back(action);
       }
       UIAction<UIObject*> *front() {
           UIAction<UIObject*> *a = new UIAction(this);
           
           if (actions.size() > 0) return actions.front();
           else return a;
       }
       void subtract() {
           if (actions.size() > 0) {
               delete front();
               actions.pop_front();
           }
       }
       // May return a nullpointer
       UIObject* intersect(float x, float y, bool touching) {
           if (touching && !this->touchable) return nullptr;
           if (!visible()) return nullptr;
           
           UIObject *obj = this;
           bool intersection = (x >= obj->position.x && y >= obj->position.y) && (x < obj->position.x + obj->width && y < obj->position.y + obj->height);
                       
           return intersection ? this : nullptr;          
       }
       bool hovering(float x, float y) {
           UIObject *o = intersect(x, y, true);
           return o == this ? true : false;
       }
       void visibility(std::function<bool()> cons) {
           this->visible = cons;
       }
    protected:
       std::function<void()> clickListener, hoverListener, updateListener;
       std::function<bool()> visible;
       std::list<UIAction<UIObject*>*> actions;
       
       bool highlighted;
       bool useSetSize;
       Vec2f clickPosition;
       Style *style;
};

class Table : public UIObject {
    public:
       Table() : UIObject() {
           this->touchable = false;
           this->touchableElementsOnly = true;
           this->useSetSize = false;
       }
       Table(Drawable *back) : UIObject() {
           this->touchable = false;
           this->touchableElementsOnly = true;
           this->useSetSize = false;
           
           this->background = back;
       }
       void calculate_size();
       void add(UIObject *obj) {
            objects.push_back(obj);
       }
       void add(UIObject *obj, float x, float y) {
            obj->position.x = x;
            obj->position.y = y;
            this->add(obj);
       }
       std::list<UIObject*> get_objects() {
            return objects;
       }
       
       void draw_background(float x, float y);
       void draw_highlight(float x, float y, SDL_Color highlightColor);
       void draw_elements();
       void draw() override;
       void handle_event(SDL_Event event) override;
    protected:
       Drawable *background;
       std::list<UIObject*> objects;
       bool touchableElementsOnly;
};

void Table::calculate_size() {
    float minX = 0, minY = 0;
    float maxX = 0, maxY = 0;
    float xValues[objects.size() * 2];
    float yValues[objects.size() * 2];
    float cx = 0, cy = 0;
    float cmx = 0, cmy = 0;
   
    int i = 0;
    for (auto &object : objects) {
         xValues[i] = object->position.x;
         xValues[i + objects.size()] = object->position.x + object->width;
         
         yValues[i] = object->position.y;
         yValues[i + objects.size()] = object->position.y + object->height;
         i++;
    }
    
    for (int j = 0; j < objects.size(); j++) {
         if ((minX == 0 || minX < cx)) {
             cx = minX;
             minX = xValues[j];
         }
         if ((minY == 0 || minY < cy)) {
             cy = minY;
             minY = yValues[j];
         }
         
         if ((maxX == 0 || maxX > cmx)) {
             cmx = maxX;
             maxX = xValues[j + objects.size()];
         }
         if ((maxY == 0 || maxY > cmy)) {
             cmy = maxY;
             maxY = yValues[j + objects.size()];
         }
    }
    
    position.x = minX;
    position.y = minY;
    width = maxX - minX;
    height = maxY - minY;
};

void Table::draw_background(float x, float y) {
    if (background == nullptr) return;
    background->draw(x, y, width, height, 0.4f);
};

void Table::draw_highlight(float x, float y, SDL_Color highlight) {
    if (!this->touchable || this->touchableElementsOnly || this->style == nullptr) return;
    
    style->draw(x, y, width, height, highlight);
};

void Table::draw_elements() {
    for (auto &element : objects) {
        element->draw();
    }
};

void Table::draw() {
    if (!this->useSetSize) {
        this->calculate_size();
    }
    
    if (this->useSetSize) {
        for (auto &object : objects) {
             object->relativePosition.x = this->position.x + this->width / 2;
             object->relativePosition.y = this->position.y + this->height / 2;
             
             object->set_position_centered(object->relativePosition);
        }
    }
    
    if (this->visible()) {
        draw_background(position.x, position.y);
        if (this->highlighted) {
            draw_highlight(position.x, position.y, highlightColor);
        }
        draw_elements();
    }
};
void Table::handle_event(SDL_Event event) {
    if (this->touchableElementsOnly) {
        for (auto &element : objects) {
             element->handle_event(event);
        }
    } else {
        int px, py;
        SDL_GetMouseState(&px, &py);
        bool intersecting = hovering(px, py);
           
        if (intersecting) { 
            hoverListener();
               
            if (event.type == SDL_MOUSEMOTION) {
                highlighted = true;
                Vars::focused = true;
            }
                  
            if (event.type == SDL_MOUSEBUTTONUP) {
                clickPosition.x = px;
                clickPosition.y = py;
                   
                clickListener();
                highlighted = false;
                Vars::focused = false;
            }
        } else {
            highlighted = false;
            Vars::focused = false;
        }
    } 
};

class Label : public UIObject {
    public:
       const char *text;
       Label() : UIObject() {
           this->touchable = false;
       }
       Label(const char *text) : UIObject() {
           this->touchable = false;
           set_text(text);
           this->labelStyle = new LabelStyle();
       }
       Label(const char *text, LabelStyle *style) : UIObject() {
           this->touchable = false;
           set_text(text);
           this->labelStyle = style;
       }
       void set_size() {
           if (textTexture != nullptr) {
               int tw, th;
               SDL_QueryTexture(textTexture, NULL, NULL, &tw, &th);
               
               // Minimum lengths
               width = tw;
               height = th;
           }
       }
       void set_text(const char *to) {
           this->text = to;
           textTexture = load_text(this->text);
           set_size();
       }
       void draw() override;
    protected:
       LabelStyle *labelStyle;
       SDL_Texture *textTexture;
};
void Label::draw() {
    if (textTexture != nullptr) {
        if (labelStyle != nullptr) {
            Draw::mix_color(textTexture, labelStyle->tint);
            Draw::alpha(textTexture, alpha * 255);
            
            if (labelStyle->background != nullptr) {
                labelStyle->background->draw(position.x, position.y, width, height, this->alpha);
            }
        }
        Draw::text(textTexture, position.x + width / 2, position.y + height / 2);
    }
};

class Button : public Table {
    public:
       Button(float width, float height, std::function<void()> clicked) : Table() {
           this->width = width;
           this->height = height;
           this->clickListener = clicked;
           this->touchable = true;
           this->touchableElementsOnly = false;
           
           this->useSetSize = true;
           
           this->background = Styles::black;
           this->style = new DefaultButtonStyle();
           this->add(new Label("Restart"));
       }
};

class UI {
    public:
       static UI &get()
       {
           static UI ins;
           return ins;
       }
       void add(UIObject *object);
       void add(UIObject *object, float x, float y);
       void load();
       
       void handle_event(SDL_Event event);
       void update(float timeTook);
       void render();
    protected:
       std::vector<UIObject*> objects;
       
    private:
        UI() {}
        ~UI() {}
    public:
        UI(UI const&) = delete;
        void operator = (UI const&) = delete; 
     
};

void UI::add(UIObject *object) {
    objects.push_back(object);
};
void UI::add(UIObject *object, float x, float y) {
    Vec2f p = { x, y };
    object->set_position_centered(p);
    
    add(object);
};

void UI::handle_event(SDL_Event event) {
    for (auto &object : objects) {
        object->handle_event(event);
    }
};
void UI::update(float timeTook) {
    for (auto &object : objects) {
        object->act(timeTook);
    }
};
void UI::render() {
    for (auto &object : objects) {
        object->draw();
    }
};

void UI::load() {
    Button *b = new Button(100, 40, [&](){
        Vec2f pos = Vars::lastPlacePosition;
        Vars::player->place(pos.x, pos.y);
        Vars::player->reset();
        
        Vars::load_level(Vars::currentLevel->name, false);
    });
    b->visibility([&]() -> bool {
        return Vars::player->position.y > 800;
    });
    add(b, 550, 50);
    /*
    Table *t = new Table(Styles::black);
    t->add(new Label("Test1"), 40, 40);
    
    Table *t2 = new Table(Styles::black);
    t2->add(new Label("Test2"), 200, 100);
    
    Table *t3 = new Table(Styles::aluminium->background);
    t3->add(t);
    t3->add(t2);
    
    add(t3);
    */
};

void Assets::load(LoadStages stage) {
    auto add = [&](WorldObject *obj) -> WorldObject* {
        if (currentLevel == nullptr) {
            printf("Could not find a level for the object. Switching to the default rectangle...");
            return new Rectangle("default-rectangle", 100, 40, 0);
        }
        return obj;
    };
    
    auto line = [&](float x2, float y2, int pointing) -> Line* {
        Line *l = new Line({0, 0}, {x2, y2});
        l->side = pointing;
        
        return (Line*) add(l);
    };
    auto ball = [&](const char *spriteName, float radius, float mass) -> Ball* {
        Ball *b = new Ball(spriteName, radius, mass);
        return (Ball*) add(b);
    };
    auto rectangle = [&](const char *spriteName, float width, float height, float angle) -> Rectangle* {
        Rectangle *r = new Rectangle(spriteName, width, height, angle);
        return (Rectangle*) add(r);
    };
    auto trigger = [&](float width, float height, float angle, std::function<void(WorldObject*)> cons) -> Trigger* {
        Trigger *t = new Trigger(width, height, angle, cons);
        return (Trigger*) add(t);
    };
    auto water = [&](float width, float height, float angle) -> Liquid* {
        Liquid *t = new Liquid({ 19, 26, 254 }, 0.1f, width, height, angle);
        return (Liquid*) add(t);
    };
    auto waterExpanse = [&]() -> Liquid* {
        Liquid *t = new Liquid({ 19, 26, 254 }, 0.2f, 20000, 5000, 0);
        return (Liquid*) add(t);
    };
    auto flag = [&](const char *levelName, float angle) -> Flag* {
        Flag *f = new Flag(levelName, angle);
        return (Flag*) add(f);
    };
    auto balloon = [&]() -> Balloon* {
        Balloon *b = new Balloon(1.0f);
        b->generate_points();
        
        return (Balloon*) add(b);
    };
    auto level = [&](const char *name) -> Level* { 
        Level *l = new Level(name);
        currentLevel = l;
        l->add(waterExpanse(), -10000, 1000);
        
        return l;
    };
         
    switch (stage) {
         case TEXTURES:
              add_texture("aluminium-ball", "aluminium-ball.png");
              add_texture("wooden-ball", "wooden-ball.png");
              add_texture("wooden-plank", "wooden-plank.png");
              add_texture("wooden-beam", "wooden-beam.png");
              add_texture("flag", "flag.png");
              add_texture("water", "water.png");
              
              add_texture("white-texture", "white-texture.png");
              add_texture("white-ui", "white-ui.png");
              add_texture("default-rectangle", "default-rectangle.png");
              break;
              
         case LEVELS:
              Level *l1 = level("Prologue");
              l1->set_start_position(200, -50);
              l1->add(rectangle("wooden-beam", 1000, 100, 0), 0, 0);
              l1->add(rectangle("wooden-plank", 140, 40, 0), 1100, -100);
              l1->add(rectangle("wooden-plank", 120, 40, 0), 1200, -200);
              
              l1->add(rectangle("wooden-plank", 700, 40, -10), 300, -200);
              l1->add(rectangle("wooden-plank", 320, 40, 0), 0, -140);
              
              l1->add(rectangle("wooden-beam", 80, 20, 0), 220, -350); 
              l1->add(rectangle("wooden-beam", 80, 20, 0), 40, -280); 
              l1->add(rectangle("wooden-beam", 300, 50, 0), 0, -190);
              
              l1->add(rectangle("wooden-plank", 200, 40, 0), 320, -450);
              l1->add(rectangle("wooden-plank", 200, 100, 0), 320, -595);
              
              l1->add(rectangle("wooden-plank", 140, 40, 0), 720, -440);
        
              l1->add(trigger(200, 400, 0, [&](WorldObject *object) {
                  ActionProcessor::get().add(new CameraMoveAction({180, -270}, 4.0f));
                  ActionProcessor::get().add(new CameraMoveAction({800, -450}, 4.0f));
                  ActionProcessor::get().add(new CameraMoveAction(2.5f));
              }), 400, -400);
               
              l1->add(flag("basin", 0), 780, -500);
              
              add_level(l1, "prologue");
              
              Level *l2 = level("Wooden Basin");
              l2->set_start_position(-700, -800);
              l2->add(rectangle("wooden-beam", 1000, 40, 0), 0, 0);
              l2->add(rectangle("wooden-plank", 300, 40, 40), 300, -300);
              l2->add(rectangle("wooden-plank", 200, 40, -50), 650, -500);
            
              for (int i = 0; i < 10; i++)
                   l2->add(ball("wooden-ball", 16, 1.2), 160 + i * 20, -1000);
                   
              for (int i = 0; i < 1; i++)    
                  l2->add(balloon(), -100 + i * 100, -900 - i * 100);
                  
              l2->add(flag("win", 0), 780, -60);
              l2->add(rectangle("wooden-beam", 400, 40, 0), 300, -740);
              
              l2->add(water(1000, 700, 0), 0, -700);
              l2->add(rectangle("wooden-plank", 40, 740, 0), 0, -740);
              l2->add(rectangle("wooden-plank", 40, 1500, 0), 960, -1500);
              l2->add(rectangle("wooden-beam", 1000, 40, 0), -1000, -740);
              
              l2->add(trigger(200, 400, 0, [&](WorldObject *object) {
                  ActionProcessor::get().add(new CameraMoveAction({500, -800}, 4.0f));
                  ActionProcessor::get().add(new CameraMoveAction({500, -200}, 4.0f));
                  ActionProcessor::get().add(new CameraMoveAction(4.0f));
              }), -350, -1140);
              
              add_level(l2, "basin");
              
              Level *l3 = level("Epilogue");
              l3->set_start_position(0, -100);
              l3->add(rectangle("wooden-beam", 1000, 40, 0), -500, 0);
              
              add_level(l3, "win");
              break;
    }
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
    public:
       void init() override {
           displayName = "Aluminium";
       } 
       void load() override {
           Assets::get().load(TEXTURES);
           Assets::get().load(LEVELS);
           
           Styles::load();
           UI::get().load();
           
           load_level(new Ball("aluminium-ball", 16.0f, 1.7f), "prologue");
       }
    
       void handle_event(SDL_Event ev) override {
           if (ActionProcessor::get().started() && !ActionProcessor::get().front()->allowPlayerControl) {
               return;
           }
           
           if (!Vars::focused) {
               int cx = 0, cy = 0;
               SDL_GetMouseState(&cx, &cy);
           
               float f = cx > SCREEN_WIDTH / 2 ? 4 : -4;
               Vars::player->vel.x += f;
               
               if (Vars::player->colliding != nullptr) {
                   if (cy < SCREEN_WIDTH / 2) { 
                       Vars::player->jump(300, Vars::player->colliding);
                       Vars::player->colliding = nullptr;
                   }
               }
           }
           UI::get().handle_event(ev);
       }
       void update(float timeTook) override { 
           for (auto &obj : Vars::currentLevel->get_objects()) {
                obj->update(timeTook);
           }
           if (ActionProcessor::get().started()) { 
               if (ActionProcessor::get().front()->canMoveCamera) {
                   Projection::adjust_camera(Vars::player->position.x, Vars::player->position.y);
               }
           }
           else Projection::adjust_camera(Vars::player->position.x, Vars::player->position.y);
           
           ActionProcessor::get().update(timeTook);
           UI::get().update(timeTook);
           
           // Collision detection
           for (auto &obj : Vars::currentLevel->get_objects()) {
                const char *name = obj->name;
                if (name == "ball") {
                     Ball *ball = (Ball*) obj;
                     for (auto &other : Vars::currentLevel->get_objects()) {
                          if (ball->index != other->index) {
                              ball->do_collision(other);
                          }
                     }
                }
                if (name == "balloon") {
                     Balloon *balloon = (Balloon*) obj;
                     for (auto &other : Vars::currentLevel->get_objects()) {
                          if (balloon->index != other->index) {
                              for (auto &vert : balloon->get_vertices()) {
                                   vert->do_collision(other);
                              }
                              if (other->name == "balloon") {
                                   Balloon *b = (Balloon*) other;
                                   for (auto &vert : balloon->get_vertices()) {
                                        for (auto &vert2 : b->get_vertices()) {
                                             vert->do_vertice_collision(vert2);
                                        }
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
           
           for (auto &obj : Vars::currentLevel->get_objects()) {
                obj->render();
           }
           ActionProcessor::get().render();
           // UI is drawn as overlay
           UI::get().render();
       }
       void load_level(Ball *ball, const char *levelName) {
           Vars::player = ball;
           Vars::load_level(levelName, true);   
       }
       void load_level(const char *levelName) { 
           Vars::load_level(levelName, false);
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