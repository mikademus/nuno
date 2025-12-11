#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <sstream>
#include <iostream>
#include <memory>
#include <stdexcept>

// Value types supported
enum class value_type 
{
    string,
    integer,
    decimal,
    boolean,
    date,
    string_array,
    int_array,
    float_array
};

// Variant to hold different value types
using value =   std::variant
                <
                    std::string,
                    int64_t,
                    double,
                    bool,
                    std::vector<std::string>,
                    std::vector<int64_t>,
                    std::vector<double>
                >;

// Column definition for tables
struct column 
{
    std::string name;
    value_type type;
};

// Table row (ordered values matching columns)
using table_row = std::vector<value>;

// Category/Subcategory node
struct category 
{
    std::string name;
    std::map<std::string, std::string> key_values;
    std::vector<column> table_columns;
    std::vector<table_row> table_rows;
    std::map<std::string, std::unique_ptr<category>> subcategories;
};

// Root document
struct document 
{
    std::map<std::string, std::unique_ptr<category>> categories;
};

class config_parser 
{
public:
    document parse(const std::string& input) 
    {
        lines_ = split_lines(input);
        line_num_ = 0;
        doc_ = document{};
        category_stack_.clear();
        current_table_.clear();
        in_table_mode_ = false;
        
        while (line_num_ < lines_.size()) 
        {
            parse_line(lines_[line_num_]);
            ++line_num_;
        }
        
        return std::move(doc_);
    }

private:
    std::vector<std::string> lines_;
    size_t line_num_ = 0;
    document doc_;
    std::vector<category*> category_stack_;
    std::vector<column> current_table_;
    bool in_table_mode_ = false;

    std::vector<std::string> split_lines(const std::string& input) 
    {
        std::vector<std::string> result;
        std::istringstream stream(input);
        std::string line;
        while (std::getline(stream, line))
            result.push_back(line);
        return result;
    }

    std::string trim(const std::string& str) 
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    void parse_line(const std::string& line) 
    {
        std::string trimmed = trim(line);
        
        // Empty line - ends table mode
        if (trimmed.empty()) 
        {
            in_table_mode_ = false;
            return;
        }
        
        // Comment line - ignore
        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/')
            return;
        
        // Top-level category
        if (!trimmed.empty() && trimmed.back() == ':' && trimmed[0] != ':' && trimmed[0] != '#') 
        {
            parse_top_level_category(trimmed);
            return;
        }
        
        // Subcategory open
        if (trimmed[0] == ':') 
        {
            parse_subcategory_open(trimmed);
            return;
        }
        
        // Subcategory close
        if (trimmed[0] == '/') 
        {
            parse_subcategory_close(trimmed);
            return;
        }
        
        // Table header
        if (trimmed[0] == '#') 
        {
            parse_table_header(trimmed);
            return;
        }
        
        // Key-value pair
        if (trimmed.find('=') != std::string::npos && !in_table_mode_) 
        {
            parse_key_value(trimmed);
            return;
        }
        
        // Table row
        if (in_table_mode_) 
        {
            parse_table_row(trimmed);
            return;
        }
        
        // Unrecognized - ignore with warning
        std::cerr << "Warning: Unrecognized line " << line_num_ << ": " << line << std::endl;
    }

    void parse_top_level_category(const std::string& line) 
    {
        std::string name = line.substr(0, line.size() - 1);
        name = trim(name);
        
        // Close all open subcategories
        category_stack_.clear();
        in_table_mode_ = false;
        current_table_.clear();
        
        // Create new top-level category
        auto cat = std::make_unique<category>();
        cat->name = name;
        category* ptr = cat.get();
        doc_.categories[name] = std::move(cat);
        category_stack_.push_back(ptr);
    }

    void parse_subcategory_open(const std::string& line) 
    {
        if (category_stack_.empty()) 
        {
            std::cerr << "Error: Subcategory without parent at line " << line_num_ << std::endl;
            return;
        }
        
        std::string name = trim(line.substr(1));
        
        category * parent = category_stack_.back();
        auto subcat = std::make_unique<category>();
        subcat->name = name;
        
        // If we're in table mode, inherit the table structure
        if (in_table_mode_)
            subcat->table_columns = current_table_;
        
        category* ptr = subcat.get();
        parent->subcategories[name] = std::move(subcat);
        category_stack_.push_back(ptr);
    }

    void parse_subcategory_close(const std::string& line) 
    {
        if (category_stack_.size() <= 1) 
        {
            // Silently ignore - might be closing an already-closed category
            return;
        }
        
        std::string name = trim(line.substr(1));
        
        // Lenient closing: if name matches, close it; if empty or "subcategory", close most recent
        if (name.empty() || name == "subcategory" || category_stack_.back()->name == name) 
        {
            category_stack_.pop_back();
        } 
        else 
        {
            // Try to find matching name in stack
            bool found = false;
            for (auto it = category_stack_.rbegin(); it != category_stack_.rend(); ++it) 
            {
                if ((*it)->name == name) 
                {
                    // Close up to this level
                    size_t pos = std::distance(category_stack_.begin(), it.base()) - 1;
                    category_stack_.erase(category_stack_.begin() + pos + 1, category_stack_.end());
                    found = true;
                    break;
                }
            }
            if (!found) 
            {
                // No match - just close most recent
                if (category_stack_.size() > 1)
                    category_stack_.pop_back();
            }
        }
    }

    void parse_table_header(const std::string & line) 
    {
        current_table_.clear();
        std::string header = trim(line.substr(1));
        
        std::istringstream stream(header);
        std::string token;
        while (stream >> token) 
        {
            column col;
            size_t colonPos = token.find(':');
            if (colonPos != std::string::npos) 
            {
                col.name = token.substr(0, colonPos);
                col.type = parse_type(token.substr(colonPos + 1));
            } 
            else 
            {
                col.name = token;
                col.type = value_type::string; // Default
            }
            current_table_.push_back(col);
        }
        
        in_table_mode_ = true;
        
        // Set columns in current category
        if (!category_stack_.empty()) 
            category_stack_.back()->table_columns = current_table_;
    }

    value_type parse_type(const std::string& typeStr) 
    {
        if (typeStr == "int") return value_type::integer;
        if (typeStr == "float") return value_type::decimal;
        if (typeStr == "bool") return value_type::boolean;
        if (typeStr == "date") return value_type::date;
        if (typeStr == "str[]") return value_type::string_array;
        if (typeStr == "int[]") return value_type::int_array;
        if (typeStr == "float[]") return value_type::float_array;
        return value_type::string;
    }

    void parse_key_value(const std::string& line) 
    {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) return;
        
        std::string key = trim(line.substr(0, eqPos));
        std::string value = trim(line.substr(eqPos + 1));
        
        if (!category_stack_.empty())
            category_stack_.back()->key_values[key] = value;
    }

    void parse_table_row(const std::string& line) 
    {
        if (current_table_.empty() || category_stack_.empty()) return;
        
        std::vector<std::string> cells = split_table_cells(line);
        
        if (cells.size() != current_table_.size()) 
        {
            std::cerr << "Warning: Row at line " << line_num_ << " has " << cells.size() 
                      << " cells but expected " << current_table_.size() << std::endl;
            // Pad or truncate to match expected size
            while (cells.size() < current_table_.size()) 
                cells.push_back("");

            if (cells.size() > current_table_.size()) 
                cells.resize(current_table_.size());
        }
        
        table_row row;
        for (size_t i = 0; i < cells.size(); i++)
            row.push_back(parse_value(cells[i], current_table_[i].type));
        
        category_stack_.back()->table_rows.push_back(std::move(row));
    }

    std::vector<std::string> split_table_cells(const std::string& line) 
    {
        std::vector<std::string> cells;
        std::string current;
        int spaces = 0;
        
        for (char c : line) 
        {
            if (c == ' ') 
            {
                ++spaces;
                if (spaces >= 2 && !current.empty()) 
                {
                    cells.push_back(trim(current));
                    current.clear();
                    spaces = 0;
                }
            } 
            else 
            {
                if (spaces == 1) current += ' ';
                spaces = 0;
                current += c;
            }
        }
        
        if (!current.empty())
            cells.push_back(trim(current));
        
        return cells;
    }

    value parse_value(const std::string& str, value_type type) 
    {
        switch (type) 
        {
            case value_type::integer:
                return static_cast<int64_t>(std::stoll(str));
            case value_type::decimal:
                return std::stod(str);
            case value_type::boolean:
                return str == "true" || str == "yes" || str == "1";
            case value_type::string_array:
                return split_array<std::string>(str, [](const std::string& s) { return s; });
            case value_type::int_array:
                return split_array<int64_t>(str, [](const std::string& s) { return std::stoll(s); });
            case value_type::float_array:
                return split_array<double>(str, [](const std::string& s) { return std::stod(s); });
            default:
                return str;
        }
    }

    template<typename T, typename F>
    std::vector<T> split_array(const std::string& str, F converter) 
    {
        std::vector<T> result;
        std::string current;
        
        for (char c : str) 
        {
            if (c == '|') 
            {
                if (!current.empty()) 
                {
                    result.push_back(converter(trim(current)));
                    current.clear();
                }
            } 
            else 
            {
                current += c;
            }
        }
        
        if (!current.empty())
            result.push_back(converter(trim(current)));
        
        return result;
    }
};


// Example usage demonstrating the format
int main() 
{
    std::string config = R"(
// Game Configuration Example
// This showcases a clean, human-readable format for hierarchical data

//----------------------------------------------------------------------------
// The two first categories (server and monsters) will demonstrate top-level
// categories, tables, key/values, and subcategories.
//----------------------------------------------------------------------------

server:
    # region:str  address:str               port:int  max_players:int  active:bool
      us-east     game-us-east.example.com  7777      64               true
      us-west     game-us-west.example.com  7777      64               true
      eu-central  game-eu.example.com       7778      128              true
    
    // Metadata about server configuration
    version = 2.1.5
    last_updated = 2025-12-11
    admin_contact = ops@example.com
    
  :load_balancing
    strategy = round-robin
    health_check_interval = 30
    retry_attempts = 3
  /load_balancing
/server
// ^ The top-level category can be explicitly closed at its end.

// You can be creative with decorating the tables (or any structure) for readability:
characters:
    # id:str         class:str   base_hp:int  base_mana:int  speed:float  start_skills:str[]
   //-----------------------------------------------------------------------------------------
      warrior_m      warrior     150          20             1.0          slash|block|taunt
      mage_f         mage        80           200            0.85         fireball|ice_shield|teleport
      rogue_m        rogue       100          50             1.3          backstab|stealth|pickpocket
      cleric_f       cleric      110          150            0.95         heal|bless|smite
    
  :warrior
    description = Heavily armored melee fighter with high survivability
    difficulty = beginner
    
    # ability_id:str    cooldown:float  mana_cost:int  damage:int
   //----------------------------------------------------------------------------------
      slash             2.5             10             35
      block             8.0             15             0
      taunt             12.0            20             5
  /warrior
  
  :mage
    description = Glass cannon spellcaster with devastating ranged attacks
    difficulty = advanced
    
    # ability_id:str    cooldown:float  mana_cost:int  damage:int
   //----------------------------------------------------------------------------------
      fireball          4.0             40             85
      ice_shield        15.0            60             0
      teleport          20.0            80             0
  /mage

// ^ Here we're skipping closing the characters category since
//   the new top-level category (below) closes all earlier nodes.


//----------------------------------------------------------------------------
// The monsters category demonstrates that subcategories can be
// specified inside an ongoing table without closing it.
//----------------------------------------------------------------------------

monsters:
    # id:int  name:str         count:int
      1       bat              13
      2       rat              42
  // goblins is a sub-category under monster continuing the same table format:
  :goblins
      3       green goblin     123
      4       red goblin       456
  /goblins
  :undead
      5       skeleton         314
      6       zombie           999
  /undead
      // the base category continues here
      7       kobold           3
      8       orc              10


items:
    # item_id:str          name:str               type:str    value:int  weight:float  stackable:bool
      potion_health_small  Minor Health Potion    consumable  15         0.2           true
      potion_health_large  Greater Health Potion  consumable  75         0.3           true
      scroll_teleport      Scroll of Teleport     consumable  200        0.1           true
    
  :weapons
    // Melee and magical weapons available to players
    # item_id:str       name:str          damage:int  durability:int  required_level:int
      sword_iron        Iron Sword        25          100             1
      sword_steel       Steel Longsword   45          200             5
      staff_oak         Oak Staff         20          80              1
      staff_arcane      Arcane Staff      60          150             8
      dagger_bronze     Bronze Dagger     18          60              1
  /weapons
  
  :armor
    # item_id:str       name:str              defense:int  weight:float  required_level:int
      helmet_leather    Leather Cap           5            1.2           1
      helmet_iron       Iron Helmet           15           3.5           4
      chest_cloth       Cloth Robe            8            2.0           1
      chest_chainmail   Chainmail Hauberk     25           12.0          6
  /armor
/items

game_settings:
    title = Epic Quest Adventures
    version = 1.2.0
    release_date = 2025-11-15
    
    // Graphics and performance
    default_resolution = 1920x1080
    target_fps = 60
    vsync_enabled = true
    
  :difficulty_modifiers
    # level:str   damage_multiplier:float  health_multiplier:float  xp_multiplier:float
      easy        0.75                     1.5                      0.8
      normal      1.0                      1.0                      1.0
      hard        1.5                      0.75                     1.25
      nightmare   2.0                      0.5                      1.5
  /difficulty_modifiers
  
  :audio
    master_volume = 0.8
    music_volume = 0.6
    sfx_volume = 0.9
    voice_volume = 1.0
    
    // Audio file paths
    main_theme = audio/music/main_theme.ogg
    battle_theme = audio/music/battle_intense.ogg
  /audio
/game_settings
)";

    config_parser parser;
    document doc = parser.parse(config);
    
    std::cout << "=== Hierarchical Config Format Demo ===" << std::endl;
    std::cout << "Parsed " << doc.categories.size() << " top-level categories" << std::endl << std::endl;
    
    // Show server data
    if (doc.categories.count("server")) 
    {
        auto & server = doc.categories["server"];
        std::cout << "Server regions: " << server->table_rows.size() << " entries" << std::endl;
        std::cout << "Server version: " << server->key_values["version"] << std::endl;
        std::cout << "Load balancing strategy: " 
                  << server->subcategories["load_balancing"]->key_values["strategy"] << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show character classes
    if (doc.categories.count("characters")) 
    {
        auto & chars = doc.categories["characters"];
        std::cout << "Character classes: " << chars->table_rows.size() << " available" << std::endl;
        std::cout << "Warrior abilities: " 
                  << chars->subcategories["warrior"]->table_rows.size() << std::endl;
        std::cout << "Mage abilities: " 
                  << chars->subcategories["mage"]->table_rows.size() << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show monsters with subcategories
    if (doc.categories.count("monsters")) 
    {
        auto & monsters = doc.categories["monsters"];
        std::cout << "Monsters (base): " << monsters->table_rows.size() << " entries" << std::endl;
        std::cout << "Goblins: " << monsters->subcategories["goblins"]->table_rows.size() << std::endl;
        std::cout << "Undead: " << monsters->subcategories["undead"]->table_rows.size() << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show items
    if (doc.categories.count("items")) 
    {
        auto & items = doc.categories["items"];
        std::cout << "Base items: " << items->table_rows.size() << std::endl;
        std::cout << "Weapons: " << items->subcategories["weapons"]->table_rows.size() << std::endl;
        std::cout << "Armor pieces: " << items->subcategories["armor"]->table_rows.size() << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show game settings
    if (doc.categories.count("game_settings")) 
    {
        auto & settings = doc.categories["game_settings"];
        std::cout << "Game: " << settings->key_values["title"] << std::endl;
        std::cout << "Version: " << settings->key_values["version"] << std::endl;
        std::cout << "Difficulty levels: " 
                  << settings->subcategories["difficulty_modifiers"]->table_rows.size() << std::endl;
    }
    
    return 0;
}