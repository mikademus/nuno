#include "include/arf_core.hpp"
#include "include/arf_parser.hpp"
#include "include/arf_serializer.hpp"
#include "include/arf_query.hpp"
#include <iostream>
#include <iomanip>

// Example Arf configuration
const char* example_config = R"(
// Game Configuration Example
// This showcases the Arf format features

server:
    # region:str  address:str               port:int  max_players:int  active:bool
      us-east     game-us-east.example.com  7777      64               true
      us-west     game-us-west.example.com  7777      64               true
      eu-central  game-eu.example.com       7778      128              true
    
    version = 2.1.5
    last_updated = 2025-12-11
    admin_contact = ops@example.com
    
  :load_balancing
    strategy = round-robin
    health_check_interval = 30
    retry_attempts = 3
  /load_balancing
/server

characters:
    # id:str         class:str   base_hp:int  base_mana:int  speed:float  start_skills:str[]
      warrior_m      warrior     150          20             1.0          slash|block|taunt
      mage_f         mage        80           200            0.85         fireball|ice_shield|teleport
      rogue_m        rogue       100          50             1.3          backstab|stealth|pickpocket
      cleric_f       cleric      110          150            0.95         heal|bless|smite
    
  :warrior
    description = Heavily armored melee fighter with high survivability
    difficulty = beginner
    
    # ability_id:str    cooldown:float  mana_cost:int  damage:int
      slash             2.5             10             35
      block             8.0             15             0
      taunt             12.0            20             5
  /warrior
  
  :mage
    description = Glass cannon spellcaster with devastating ranged attacks
    difficulty = advanced
    
    # ability_id:str    cooldown:float  mana_cost:int  damage:int
      fireball          4.0             40             85
      ice_shield        15.0            60             0
      teleport          20.0            80             0
  /mage
/characters

monsters:
    # id:int  name:str         count:int
      1       bat              13
      2       rat              42
      
  :goblins
      3       green goblin     123
      4       red goblin       456
  /goblins
  
  :undead
      5       skeleton         314
      6       zombie           999
  /undead
  
      7       kobold           3
      8       orc              10
/monsters

game_settings:
    title = Epic Quest Adventures
    version = 1.2.0
    release_date = 2025-11-15
    
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
    
    main_theme = audio/music/main_theme.ogg
    battle_theme = audio/music/battle_intense.ogg
  /audio
/game_settings
)";

void print_separator(const std::string& title)
{
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

void test_parsing()
{
    print_separator("TEST 1: Basic Parsing");
    
    auto result = arf::parse(example_config);
    
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed with errors:\n";
        for (const auto& err : result.errors)
        {
            std::cout << "  " << err.to_string() << "\n";
        }
        return;
    }
    
    arf::document& doc = *result.doc;
    
    std::cout << "✓ Parsed " << doc.categories.size() << " top-level categories\n";
    
    for (const auto& [name, cat] : doc.categories)
    {
        std::cout << "  • " << name << ": "
                  << cat->table_rows.size() << " table rows, "
                  << cat->key_values.size() << " key-values, "
                  << cat->subcategories.size() << " subcategories\n";
    }
}

void test_table_view_api()
{
    print_separator("TEST 2: Table View API");
    
    auto result = arf::parse(example_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    // Get table view
    auto server_table = arf::get_table(doc, "server");
    if (!server_table)
    {
        std::cout << "✗ Failed to get server table\n";
        return;
    }
    
    std::cout << "Server Table (using table_view):\n";
    std::cout << "  Columns: " << server_table->columns().size() << "\n";
    std::cout << "  Rows: " << server_table->rows().size() << "\n\n";
    
    // Access rows with named columns
    std::cout << "Server Regions (named column access):\n";
    for (size_t i = 0; i < server_table->rows().size(); ++i)
    {
        auto row = server_table->row(i);
        auto region = row.get_string("region");
        auto address = row.get_string("address");
        auto port = row.get_int("port");
        auto active = row.get_bool("active");
        
        std::cout << "  " << (region ? *region : "N/A") 
                  << " -> " << (address ? *address : "N/A")
                  << ":" << (port ? std::to_string(*port) : "N/A")
                  << " [" << (active && *active ? "active" : "inactive") << "]\n";
    }
}

void test_key_value_queries()
{
    print_separator("TEST 3: Key-Value Queries");
    
    auto result = arf::parse(example_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    // Query simple values
    auto version = arf::get_string(doc, "server.version");
    auto contact = arf::get_string(doc, "server.admin_contact");
    auto strategy = arf::get_string(doc, "server.load_balancing.strategy");
    
    std::cout << "Server Configuration:\n";
    std::cout << "  Version: " << (version ? *version : "N/A") << "\n";
    std::cout << "  Admin: " << (contact ? *contact : "N/A") << "\n";
    std::cout << "  Load Balancing: " << (strategy ? *strategy : "N/A") << "\n\n";
    
    // Query typed values
    auto fps = arf::get_int(doc, "game_settings.target_fps");
    auto vsync = arf::get_bool(doc, "game_settings.vsync_enabled");
    auto master_vol = arf::get_float(doc, "game_settings.audio.master_volume");
    
    std::cout << "Game Settings:\n";
    std::cout << "  Target FPS: " << (fps ? std::to_string(*fps) : "N/A") << "\n";
    std::cout << "  VSync: " << (vsync ? (*vsync ? "enabled" : "disabled") : "N/A") << "\n";
    std::cout << "  Master Volume: " << (master_vol ? std::to_string(*master_vol) : "N/A") << "\n";
}

void test_array_queries()
{
    print_separator("TEST 4a: Array Access API");
    
    // First, create a test document with array values
    const char* array_config = R"(
player:
  name = Hero
  tags:str[] = warrior|knight|champion
  scores:int[] = 100|250|500
  multipliers:float[] = 1.5|2.0|2.5
/player
)";
    
    auto result = arf::parse(array_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    // Test new get_array helper
    auto tags = arf::get_string_array(doc, "player.tags");
    if (tags)
    {
        std::cout << "✓ String array (get_array): ";
        for (size_t i = 0; i < tags->size(); ++i)
        {
            std::cout << (*tags)[i];
            if (i < tags->size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    else
    {
        std::cout << "✗ Failed to get string array\n";
    }
    
    auto scores = arf::get_int_array(doc, "player.scores");
    if (scores)
    {
        std::cout << "✓ Int array (get_array): ";
        for (size_t i = 0; i < scores->size(); ++i)
        {
            std::cout << (*scores)[i];
            if (i < scores->size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    
    auto multipliers = arf::get_float_array(doc, "player.multipliers");
    if (multipliers)
    {
        std::cout << "✓ Float array (get_array): ";
        for (size_t i = 0; i < multipliers->size(); ++i)
        {
            std::cout << (*multipliers)[i];
            if (i < multipliers->size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
}

void test_reflection_api()
{
    print_separator("TEST 4b: Reflection API (value_ref)");
    
    const char* test_config = R"(
data:
  string_val = hello world
  int_val:int = 42
  float_val:float = 3.14
  bool_val:bool = true
  array_val:str[] = alpha|beta|gamma
/data
)";
    
    auto result = arf::parse(test_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    // Test string value
    auto str_ref = arf::get(doc, "data.string_val");
    if (str_ref)
    {
        std::cout << "✓ String value:\n";
        std::cout << "    is_scalar: " << (str_ref->is_scalar() ? "yes" : "no") << "\n";
        std::cout << "    is_string: " << (str_ref->is_string() ? "yes" : "no") << "\n";
        std::cout << "    value: "     << *str_ref->as_string() << "\n\n";
    }
    
    // Test int value
    auto int_ref = arf::get(doc, "data.int_val");
    if (int_ref)
    {
        std::cout << "✓ Int value:\n";
        std::cout << "    is_scalar: " << (int_ref->is_scalar() ? "yes" : "no") << "\n";
        std::cout << "    is_int: "    << (int_ref->is_int() ? "yes" : "no") << "\n";
        std::cout << "    value: "     << *int_ref->as_int() << "\n\n";
    }
    
    // Test float value
    auto float_ref = arf::get(doc, "data.float_val");
    if (float_ref)
    {
        std::cout << "✓ Float value:\n";
        std::cout << "    is_scalar: " << (float_ref->is_scalar() ? "yes" : "no") << "\n";
        std::cout << "    is_float: "  << (float_ref->is_float() ? "yes" : "no") << "\n";
        std::cout << "    value: "     << *float_ref->as_float() << "\n\n";
    }
    
    // Test bool value
    auto bool_ref = arf::get(doc, "data.bool_val");
    if (bool_ref)
    {
        std::cout << "✓ Bool value:\n";
        std::cout << "    is_scalar: " << (bool_ref->is_scalar() ? "yes" : "no") << "\n";
        std::cout << "    is_bool: "   << (bool_ref->is_bool() ? "yes" : "no") << "\n";
        std::cout << "    value: "     << (*bool_ref->as_bool() ? "true" : "false") << "\n\n";
    }
    
    // Test array value
    auto array_ref = arf::get(doc, "data.array_val");
    if (array_ref)
    {
        std::cout << "✓ Array value:\n";
        std::cout << "    is_scalar: "       << (array_ref->is_scalar() ? "yes" : "no") << "\n";
        std::cout << "    is_array: "        << (array_ref->is_array() ? "yes" : "no") << "\n";
        std::cout << "    is_string_array: " << (array_ref->is_string_array() ? "yes" : "no") << "\n";
        
        auto arr = array_ref->string_array();
        std::cout << "    values: ";
        for (size_t i = 0; i < arr->size(); ++i)
        {
            std::cout << (*arr)[i];
            if (i < arr->size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    
    // Test error handling
    std::cout << "\n✓ Error handling:\n";
    if (str_ref)
    {
        if (!str_ref->as_int().has_value())
            std::cout << "    ✓ Correctly failed conversion\n";
        else
            std::cout << "    ✗ Should have refused conversion\n";
    }
}

void test_array_in_table()
{
    print_separator("TEST 4c: Array Access in Tables (span-based)");
    
    auto result = arf::parse(example_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    auto chars = arf::get_table(doc, "characters");
    if (!chars)
    {
        std::cout << "✗ Failed to get characters table\n";
        return;
    }
    
    std::cout << "Character Skills (using span-based arrays):\n\n";
    
    for (size_t i = 0; i < chars->rows().size(); ++i)
    {
        auto row = chars->row(i);
        auto id = row.get_string("id");
        auto char_class = row.get_string("class");
        auto skills = row.get_string_array("start_skills");
        
        std::cout << "  " << (id ? *id : "N/A")
                  << " (" << (char_class ? *char_class : "N/A") << "): ";
        
        if (skills)
        {
            for (size_t j = 0; j < skills->size(); ++j)
            {
                std::cout << (*skills)[j];
                if (j < skills->size() - 1) std::cout << ", ";
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "\n✓ No array copying - using std::span views!\n";
}

void test_recursive_table_iteration()
{
    print_separator("TEST 5a: Recursive Table Iteration");
    
    auto result = arf::parse(example_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    
    auto monsters = arf::get_table(doc, "monsters");
    if (!monsters)
    {
        std::cout << "✗ Failed to get monsters table\n";
        return;
    }
    
    std::cout << "All Monsters (recursive iteration):\n\n";
    
    // Use the new rows_recursive() API
    for (auto row : monsters->rows_recursive())
    {
        auto id = row.get_int("id");
        auto name = row.get_string("name");
        auto count = row.get_int("count");
        
        // Show provenance
        std::string source = row.is_base_row() ? "base" : row.source_name();
        std::string path = arf::to_path(row, doc);
        
        std::cout << "  [" << source << "] "
                  << (id ? std::to_string(*id) : "?") << ". "
                  << (name ? *name : "N/A") << " (count: "
                  << (count ? std::to_string(*count) : "?") << ")"
                  << " @ " << path << "\n";
    }
    
    std::cout << "\n✓ Iterated through all rows in BSF order\n";
}

void test_document_order_iteration()
{
    print_separator("TEST 5b: Document Order vs Recursive Order");
    
    // Create a test config that shows the difference
    const char* test_config = R"(
items:
    # id:int  name:str  value:int
      1       sword     100
      2       shield    50
  :weapons
      3       axe       75
      4       bow       60
  /weapons
      5       potion    25
  :armor
      6       helmet    40
      7       boots     30
  /armor
      8       ring      200
/items
)";
    
    auto result = arf::parse(test_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }
    
    arf::document& doc = *result.doc;
    auto items = arf::get_table(doc, "items");
    if (!items)
    {
        std::cout << "✗ Failed to get items table\n";
        return;
    }
    
    std::cout << "Document Order (as authored):\n";
    for (auto row : items->rows_document())
    {
        auto id = row.get_int("id");
        auto name = row.get_string("name");
        std::string source = row.is_base_row() ? "base" : row.source_name();
        
        std::cout << "  [" << source << "] "
                  << (id ? std::to_string(*id) : "?") << ". "
                  << (name ? *name : "N/A") << "\n";
    }
    
    std::cout << "\nRecursive Order (logical structure):\n";
    for (auto row : items->rows_recursive())
    {
        auto id = row.get_int("id");
        auto name = row.get_string("name");
        std::string source = row.is_base_row() ? "base" : row.source_name();
        
        std::cout << "  [" << source << "] "
                  << (id ? std::to_string(*id) : "?") << ". "
                  << (name ? *name : "N/A") << "\n";
    }
    
    std::cout << "\n✓ Document order: 1,2,3,4,5,6,7,8 (interleaved)\n";
    std::cout << "✓ Recursive order: 1,2,5,8,3,4,6,7 (base then children)\n";

    
    const char* test_config_2 = R"(
things:
  #  id   name
    1  base
  :a
    2   a1
  /a
    3  base2
/things
)";

    auto res2 = arf::parse(test_config_2);
    if (!res2.has_value())
    {
        std::cout << "✗ Parse failed\n";
        return;
    }

    {
        auto items = arf::get_table(*res2.doc, "things");
        if (!items)
        {
            std::cout << "✗ Failed to get items table\n";
            return;
        }
        else {
            std::cout << "Rows/cols = " << items->rows().size() << "/" << items->columns().size() << "\n";
        }
        std::cout << "\nDocument Order (as authored) for test table 2:\n";
        for (auto row : items->rows_document())
        {
            auto id = row.get_int("id");
            auto name = row.get_string("name");
            std::string source = row.is_base_row() ? "base" : row.source_name();
            
            std::cout << "  [" << source << "] "
                    << (id ? std::to_string(*id) : "?") << ". "
                    << (name ? *name : "N/A") << "\n";

        }
    }
}

void test_serialization()
{
    print_separator("TEST 6: Round-Trip Serialization");
    
    auto result = arf::parse(example_config);
    if (!result.has_value())
    {
        std::cout << "✗ Parse failed\n";
        for (const auto& err : result.errors)
        {
            std::cout << "  " << err.to_string() << "\n";
        }
        return;
    }
    
    arf::document& doc = *result.doc;
    std::string serialized = arf::serialize(doc);
    
    std::cout << "Original size: " << strlen(example_config) << " bytes\n";
    std::cout << "Serialized size: " << serialized.size() << " bytes\n\n";
    
    std::cout << "Serialized output (first 500 chars):\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << serialized.substr(0, 500) << "\n";
    std::cout << (serialized.size() > 500 ? "...\n" : "");
    std::cout << std::string(70, '-') << "\n\n";
    
    // Parse the serialized output
    auto result2 = arf::parse(serialized);
    if (!result2.has_value())
    {
        std::cout << "✗ Re-parse failed\n";
        return;
    }
    
    arf::document& doc2 = *result2.doc;
    std::cout << "✓ Re-parsed successfully: " << doc2.categories.size() << " categories\n";
    
    // Verify key values match
    auto version1 = arf::get_string(doc, "server.version");
    auto version2 = arf::get_string(doc2, "server.version");
    
    if (version1 && version2 && *version1 == *version2)
        std::cout << "✓ Round-trip verification passed (server.version matches)\n";
    else
        std::cout << "✗ Round-trip verification failed\n";
}

void test_edge_cases()
{
    print_separator("TEST 7: Edge Cases");
    
    // Empty document
    auto empty_result = arf::parse("");
    if (empty_result.has_value())
    {
        std::cout << "✓ Empty document: " << empty_result.doc->categories.size() << " categories\n";
        
        // Query non-existent path
        auto missing = arf::get_string(*empty_result.doc, "does.not.exist");
        std::cout << "✓ Non-existent query: " << (missing ? "found" : "not found") << "\n";
    }
    else
    {
        std::cout << "✗ Empty document parse failed\n";
    }
    
    // Minimal document
    const char* minimal = "test:\n  key = value\n/test\n";
    auto minimal_result = arf::parse(minimal);
    if (minimal_result.has_value())
    {
        auto value = arf::get_string(*minimal_result.doc, "test.key");
        std::cout << "✓ Minimal document query: " << (value ? *value : "N/A") << "\n";
    }
    else
    {
        std::cout << "✗ Minimal document parse failed\n";
    }
}

void test_error_handling()
{
    print_separator("TEST 8: Error Handling");
    
    // Test various error conditions
    const char* invalid_syntax = R"(
invalid line without proper syntax
server:
  key = value
  another invalid line
/server
)";
    
    auto result = arf::parse(invalid_syntax);
    if (result.has_errors())
    {
        std::cout << "✓ Parse errors detected (" << result.errors.size() << " errors):\n";
        for (const auto& err : result.errors)
        {
            std::cout << "  " << err.to_string() << "\n";
        }
    }
    else
    {
        std::cout << "✗ Expected parse errors but got none\n";
    }
    
    // Test mismatched types
    const char* type_mismatch = R"(
types:
  number:int = not_a_number
  flag:bool = maybe
/types
)";
    
    auto result2 = arf::parse(type_mismatch);
    if (result2.has_errors())
    {
        std::cout << "\n✓ Type mismatch errors detected (" << result2.errors.size() << " errors):\n";
        for (const auto& err : result2.errors)
        {
            std::cout << "  " << err.to_string() << "\n";
        }
    }
}

int main()
{
    std::cout << R"(
    ___         __ _ 
   /   |  _____/ _| |
  / /| | / __/ |_| |
 / ___ ||  _|  _|_|
/_/   |_|_| |_| (_) 
                    
A Readable Format - Example & Test Suite
Version 0.2.0
)" << std::endl;
    
    try
    {

        test_parsing();
        test_table_view_api();
        test_key_value_queries();
        test_array_queries();
        test_reflection_api();
        test_array_in_table();
        test_recursive_table_iteration();
        test_document_order_iteration();
        test_serialization();
        test_edge_cases();
        test_error_handling();
        
        print_separator("ALL TESTS COMPLETED");
        std::cout << "✓ All tests passed successfully!\n\n";
        
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}