# Arf! — A Readable Data Format for Configs and Tables

**Version 0.3.0** — February 2026 — Stable implementation with full CRUD support

| Arf! is a compact, deterministic data language designed as a human-centric alternative to JSON, YAML, and TOML that excels where literate documentation meets structured data — interweave prose, tables, and hierarchies in one human-centric format. | ![mascot](arf_mascot_small.png) |
|-------------------------------------------------------------------------------------------------------------------------------|---------------------------------------|

Arf! ("A Readable Format", also the bark of an enthusiastic small dog) is a compact, predictable, deterministic and explicitly scoped data language built for human readability without giving up structure. It mixes hierarchical categories, key/value pairs, and TOON-style tables[^1] (column-aligned, whitespace-delimited) that can be subdivided into named subsections.

The goals are simple:

* Allow key/value and tabular data
* Minimal syntax, predictable parsing, indentation-independent structure
* Be an improvement over leading alternatives:
  * Easier to write and read than JSON
  * Less fragile and surprising than YAML
  * More flexible than TOML
  * Much more expressive than plain TOON tables

[^1]: [The TOON format](https://github.com/toon-format/toon).

## Overview

### Comparison at a Glance
| Feature	                | JSON	 | YAML	 | TOML	 | TOON  | Arf!  | Notes |
| :---                    | ---   | ---   | ---   | ---   | ---   | --- |
| Indentation Sensitivity | ❌    | ✅    | ❌    | n/a   | ❌    |     |
| Comments	               | ❌	   | ✅	   | ✅	   | ⚠️    | ✅    | TOON: By convention, not by specification  |
| Free-form Prose         | ❌    | ❌    | ❌    | ❌    | ✅    | Arf!: First-class paragraphs alongside data |
| Key/values	             | ✅	   | ✅	   | ✅	   | ❌    | ✅    |     |
| Native Tables	          | ❌	   | ❌	   | ❌	   | ✅    | ✅    | TOON: CSV-style tabular arrays |
| No-Quote Strings	       | ❌	   | ✅	   | ❌	   | ✅    | ✅    |     |
| Type Annotations	       | ❌	   | ❌	   | ❌	   | ⚠️    | ✅    | TOON: Implicit/Schema-aware |
| Indentation-Independent	| ✅	   | ❌	   | ✅	   | ❌    | ✅    |     |
| Deterministic Parsing	  | ✅	   | ❌	   | ✅	   | ✅    | ✅    |     |
| Human readable          | C | A | A | A | S | Arf!: Fully human-centric |
| Human editable          | D | B | A | B | S | Arf!: Minimal syntax friction |

Arf! provides:

* Hierarchical categories using a minimal marker syntax rather than indentation
* Key/value bindings with optional type annotation
* Tables with TOON-like clarity
* Table subcategories that do not restart headers
* Explicit category closing for precision and control
* Whitespace freedom outside tables
* Fully deterministic parsing rules

Arf! aims to be the practical everyday format for configs, content files, world definitions, and structured datasets.

### Use cases

Examples of areas of particular suitability:
* **System Administration**: Configuration files that won't break on a copy-paste error and may contain natural language in-line documentation.
* **Technical Writing**: API documentation with configuration examples
* **Game Development**: Character sheets, item databases, quest trees with narrative
* **RPG Design**: Monster manuals, spell compendiums, campaign settings with lore
* **Research**: Annotated datasets where context travels with data
* **Data Science**: Small, readable datasets that require more hierarchy than a CSV but less overhead than a database.

### Documentation & APIs

**Format Specification:**
- Complete syntax guide (in this document below)

**C++ Implementation (0.3.0):**
- [Query Interface](docs/query_interface.md) — High-level data access and query DSL
- [Query Syntax](docs/query_dotpath_syntax.md) — Dot-path query expressions  
- API Reference — Inline documentation in header files

**Getting Started:**
- See "Using Arf! in C++" section below

## Philosophy: The Human-First Protocol, Built for Speed

Most modern data formats are designed solely for transmission (JSON) or system-level automation (YAML). They prioritize rigid schemas or opaque parsing rules. Arf! is built on the belief that for many tasks—configuration, world-building, and creative data entry—clarity for the human writer can co-exist with trivial parsing for the machine.

1. **Readability is Not a "Feature"; It's the Goal**<br />
In Arf!, you should be able to scan a file and understand its hierarchy through visual landmarks (colons and slashes) rather than counting invisible whitespace or matching nested braces. If you can't read it comfortably, it's a chore.
2. **The "Copy-Paste" Safety Principle**<br />
One of the greatest frustrations with indentation-based formats (like YAML or TOON) is that moving a block of text can silently change its meaning or break the parser. Arf! uses explicit scoping that preserves structural integrity no matter where you paste the data.
3. **Determinism Over "Magic"**<br />
Arf! avoids "magic" parsing. In YAML, the string NO might be converted to a boolean false automatically. In Arf!, a string is a string. We believe the computer should never have to guess what the human meant. This deterministic structure is precisely what allows for fast, simple machine parsing.
4. **Tables are First-Class Citizens**<br />
Data isn't just trees; it's often rows. Forcing tabular data into nested Key/Value pairs (JSON) or repetitive arrays (TOML) is an ergonomic failure. Arf! treats the Table as a primary construct, allowing humans to maintain the "spreadsheet view" they naturally prefer for lists of entities.
5. **Graceful Degradation**<br />
Arf! is designed to be "hand-rolled." You shouldn't need a specialized IDE plugin to write a config file. By allowing whitespace freedom outside of tables and offering simple shorthand closures (/), Arf! scales easily.

While other formats create complex problems that require complex parsers, Arf! provides elegant simplicity for both the human author and the machine reader.

## Quick Examples

### Configuration files

```scala
// Game settings and configuration
// Demonstrates Arf! used as .INI-replacement

settings:
  version = 1.0.0
  seed = 12345

  :graphics
    resolution = 1920x1080
    fullscreen = true

entities:
  # id   type      name
    0    nothing   Nothing
  :creatures
    1    wolf      Greyfang
    2    bear      Old Grizzle
  /creatures
  :humanoids
    10   villager  Maren
    11   guard     Thalk
  /humanoids
    100  boss      Demon Lord
/entities
```
This example is intentionally verbose, using explicit named closures inside a table to demonstrate scope. Shorthand syntax exists.

### Direct CSV-file replacement
```scala
  # sku      product_name         category     warehouse  qty   reorder  region
    A1001    Gaming Mouse         electronics  seattle    245   50       us
    A1002    Mechanical Keyboard  electronics  seattle    89    30       us
    B2001    Office Chair         furniture    seattle    12    5        us
    A1001    Gaming Mouse         electronics  toronto    67    20       canada
    A1002    Mechanical Keyboard  electronics  toronto    34    15       canada
    C3001    Standing Desk        furniture    toronto    8     3        canada
    A1001    Gaming Mouse         electronics  cdmx       123   40       mexico
    B2001    Office Chair         furniture    cdmx       45    10       mexico
    D4001    Monitor Arm          accessories  cdmx       78    25       mexico
```

### Improved CSV-replacement with annotations
```scala
// Product Inventory - Q4 2025
// Demonstrates Arf! as a CSV-replacement with hierarchy and annotations

inventory:

  Regional distribution breakdown across North American warehouses.
  Stock levels updated daily. Negative values indicate backorders.

  # sku      product_name         category     warehouse  qty   reorder
    A1001    Gaming Mouse         electronics  seattle    245   50
    A1002    Mechanical Keyboard  electronics  seattle    89    30
    B2001    Office Chair         furniture    seattle    12    5

  :canada
    // Canadian warehouse - duties paid, CAD pricing
    A1001    Gaming Mouse         electronics  toronto    67    20
    A1002    Mechanical Keyboard  electronics  toronto    34    15
    C3001    Standing Desk        furniture    toronto    8     3
  /

  :mexico  
    // Mexican distribution center - peso pricing
    A1001    Gaming Mouse         electronics  cdmx       123   40
    B2001    Office Chair         furniture    cdmx       45    10
    D4001    Monitor Arm          accessories  cdmx       78    25
  /

  Low stock items flagged for priority reorder.
  Reorder threshold assumes 2-week lead time.

/inventory
```

### Scientific dataset with annotations
```scala
// Experimental Results - Enzyme Kinetics Study
// Demonstrates Arf! for research data with embedded methodology

experiment:
  study_id = EK-2025-042
  enzyme = Lactate Dehydrogenase
  temperature = 37°C
  pH = 7.4

  Substrate concentration varied from 0.1 to 10.0 mM.
  Initial velocity measured via NADH absorbance at 340nm.
  All measurements performed in triplicate.

  # substrate_mM  velocity_uM_s  std_dev  n_replicates
    0.1           2.3            0.12     3
    0.5           8.7            0.34     3
    1.0           15.2           0.45     3
    2.0           24.8           0.52     3
    5.0           38.1           0.61     3
    10.0          42.3           0.58     3

  Michaelis-Menten parameters calculated via non-linear regression
  Vmax = 45.2 ± 1.3 μM/s
  Km = 1.8 ± 0.2 mM
  R² = 0.998

  :inhibitor_screen
    // Competitive inhibition assay with known inhibitors
    # compound       IC50_mM  inhibition_type
      Oxamate       2.4      competitive
      Gossypol      0.8      uncompetitive
      FX11          0.3      non-competitive
  /

/experiment
```

### Flowing document with prose and data interwoven
```scala
// Character Sheet - Theron Ashblade
// Demonstrates Arf! as a literature-with-data format

character:
  name = Theron Ashblade
  class = Ranger
  level = 7

Theron grew up in the Ashwood Forest, learning to track and hunt 
from his father. When bandits destroyed his village, he swore an 
oath of vengeance and now wanders the realm seeking justice.

  :attributes
    # stat       base    modifier
      strength   14      +2
      dexterity  18      +4
      wisdom     15      +2
  /

  :equipment
    # item             type      damage  notes
      Longbow          weapon    1d8     +1 to hit in forests
      Hunting Knife    weapon    1d4     Throwable
      Leather Armor    armor     AC 13   Silent movement
  /

Theron's tracking abilities are legendary. He can identify a 
creature's species, size, and traveling speed from footprints 
alone, even in adverse weather.

  :abilities
    # name              cost      effect
      Hunter's Mark     1 action  +1d6 damage to marked target
      Vanish            bonus     Become invisible until next turn
  /
/character
```
This example uses shorthand close notation ("/" by itself) to cut down on visual boilerplate.

## Literate Data: Prose Meets Structure

Arf! 0.3.0 introduces **paragraphs** — first-class prose blocks that live alongside your data. This enables entirely new use cases where documentation and data need to coexist. Example use cases:

### Technical Documentation
Interleave explanations with configuration:
```scala
database_config:

This configuration controls the connection pool settings.
Increase max_connections for high-traffic applications, but
be mindful of your database server's resource limits.

  # setting          value   notes
    max_connections  100     Per application instance
    timeout_ms       5000    Connection acquisition timeout
    retry_attempts   3       Before giving up

The connection pool uses a least-recently-used eviction policy.
Idle connections are closed after 30 minutes of inactivity.

  pool_behavior = LRU
  idle_timeout = 1800
/database_config
```

### RPG Content
Mix character descriptions, lore, and mechanics:
```scala
monster_manual:

The Shadow Drake is a rare subspecies of dragon that dwells in 
ancient ruins. Unlike its fire-breathing cousins, it exhales 
pure darkness that extinguishes light sources and causes despair.

  # name           cr   hp   ac   attacks
    Shadow Drake   7    85   16   Bite, Darkness Breath

  :lore
    habitat = Ancient ruins, deep caves
    alignment = Chaotic Evil
    diet = Carrion, unwary adventurers
  /

  Tales speak of a Shadow Drake that guards the Tomb of Kings,
  grown massive from centuries of feeding on treasure seekers.
/monster_manual
```

### Game Dialogue
Narrative choices with mechanical consequences:
```scala
quest_dialog:

   "Please, you must help us!" the village elder pleads.
   "Bandits have taken our harvest. Without it, we'll starve."

  :choice_1
    text = "I'll handle the bandits."
    
    # consequence    type       value
      reputation     faction    Village +10
      quest_start    id         bandit_raid
      reward         gold       50
    /

  :choice_2
    text = "Not my problem."
    
    # consequence    type       value
      reputation     faction    Village -20
      alignment      shift      -1
    /

    The elder's face falls as you turn away. You can hear
    quiet sobbing as you leave the village square.
/quest_dialog
```

**Key Benefits:**
- **Single source of truth** — Data and documentation stay synchronized
- **Human-readable** — Non-technical stakeholders can read and understand
- **Version control friendly** — Prose and data changes tracked together
- **Literate programming** — Explain *why* alongside *what*

This makes Arf! ideal for:
- Game design documents with embedded data
- Technical manuals with live configuration
- Annotated datasets for research
- Living documentation that includes examples
- RPG sourcebooks and supplements

## Using Arf! in C++

### Quick Start

**Include the framework:**
```cpp
#include "arf.hpp"
```

**Read and query a document:**
```cpp
auto ctx = arf::load_file("config.arf");
if (ctx.has_errors()) {
    for (auto& err : ctx.errors) {
        std::cerr << err.message << "\n";
    }
    return;
}

auto& doc = ctx.document;

// High-level queries
auto resolution = arf::query(doc, "settings.graphics.resolution").as_string();
auto fullscreen = arf::query(doc, "settings.graphics.fullscreen").as_boolean();

std::cout << "Resolution: " << *resolution << "\n";
std::cout << "Fullscreen: " << (*fullscreen ? "yes" : "no") << "\n";
```

**Navigate document structure:**
```cpp
auto root = doc.root();
auto settings = doc.category("settings");

// Iterate keys
for (auto key_id : settings->keys()) {
    auto key = doc.key(key_id);
    std::cout << key->name() << "\n";
}

// Access tables
auto entities_cat = doc.category("entities");
for (auto table_id : entities_cat->tables()) {
    auto table = doc.table(table_id);
    for (auto row_id : table->rows()) {
        auto row = doc.row(row_id);
        // Access cells...
    }
}
```

**Create and edit documents:**
```cpp
arf::document doc = arf::create_document();
arf::editor ed(doc);

// Create category and keys
auto settings_id = ed.append_category(doc.root()->id(), "settings");
ed.append_key(settings_id, "version", std::string("1.0.0"));
ed.append_key(settings_id, "port", 8080);

// Create typed table
auto table_id = ed.append_table(settings_id, {
    {"name", arf::value_type::string},
    {"enabled", arf::value_type::boolean}
});

// Add rows
ed.append_row(table_id, {std::string("feature_x"), true});
ed.append_row(table_id, {std::string("feature_y"), false});

// Edit existing values
auto version_key = arf::query(doc, "settings.version").key_id();
ed.set_key_value(*version_key, std::string("2.0.0"));
```

**Serialize to file:**
```cpp
std::ofstream out("output.arf");
arf::serializer s(doc);
s.write(out);
// Preserves authored structure and order
```

### Architecture Overview

The Arf! C++ implementation provides a complete document framework:

**Core Modules:**
- **Parser** (`arf_parser.hpp`) — Lexical analysis and CST generation
- **Materializer** (`arf_materialise.hpp`) — Document construction with semantic validation
- **Document** (`arf_document.hpp`) — Data model with stable IDs, views, and metadata
- **Query** (`arf_query.hpp`) — High-level ergonomic data access with query DSL
- **Reflection** (`arf_reflect.hpp`) — Low-level address-based inspection for tooling
- **Editor** (`arf_editor.hpp`) — Type-safe CRUD operations (required for document mutation)
- **Serializer** (`arf_serializer.hpp`) — Source-faithful output generation

**Key Design Features:**
- **Stable Entity IDs** — All entities have persistent, type-safe handles
- **Immutable Views** — Document internals hidden; views provide read-only access
- **Semantic Validation** — Automatic type checking with contamination tracking
- **Source Preservation** — Round-trip serialization maintains authored structure
- **Edit Tracking** — Documents know what was modified post-parse
- **Contamination Propagation** — Invalid values mark containers as contaminated

**Document Lifecycle:**
```
Source Text → Parser (CST) → Materializer → Document
                                               ↓
                                            Query / Reflect
                                               ↓
                                            Editor (mutations)
                                               ↓
                                            Serializer → Output Text
```

### Requirements & Integration

**Requirements:**
- C++20 compiler
- Header-only library
- No external dependencies

**Integration:**

Single-file include:
```cpp
#include "arf.hpp"  // All modules
```

Selective includes:
```cpp
#include "arf_core.hpp"       // Core types only
#include "arf_parser.hpp"     // Parser
#include "arf_query.hpp"      // Query interface
#include "arf_editor.hpp"     // Editor API
// etc.
```

**Build System:**
```cmake
# CMake example
target_include_directories(my_app PRIVATE path/to/arf/include)
target_compile_features(my_app PRIVATE cxx_std_20)
```

## Rationale

* JSON is rigid and noisy.
* YAML is permissive to the point of being a riddle.
* TOML is reliable but structurally repetitive.
* TOON tables are lovely but limited.

Arf! attempts to unify the strengths of each:
* JSON's predictability
* YAML's readability
* TOML's clarity
* TOON's table ergonomics
…while discarding their pain points.

# Syntax Overview
## Basic constructs
### Whitespace
Outside tables whitespace is purely cosmetic for the benefit of human readers. Writers are encouraged to use indentation, empty lines and comments to structure their files for the sake of presentation. Inside tables cells are separated by **two or more spaces**.

### Key/Value Pairs
```scala
key = value
```
#### String values
String values are raw text extending from the first ```=``` to ```end-of-line```.
Leading whitespace is trimmed; trailing whitespace is not semantically significant.
All characters other than EOL are permitted within a value.
* Strings may contain spaces, and multiple consecutive spaces are allowed.
* Strings are not delimited by quotation marks and end at EOL.
* There is no support for multi-line strings

Strings and escaping:
> [!NOTE]
> Arf! treats string values as raw text.
> No escape sequences are recognised or interpreted by the language.
> Backslashes, quotes, and other characters have no special meaning.
> Clients may impose their own conventions for interpreting string contents.

Preserving leading and trailing whitespace:
> [!NOTE]
> Users may enclose values in custom delimiters (such as quotes or brackets) to preserve leading or trailing whitespace or to encode structured content.
> Stripping or interpreting such delimiters is the responsibility of the client.

All valid:
```scala
path = C:\Games\Foo\bar\n
text = "   padded string   "
regex = ^\w+\s+\w+$
``` 

### Type annotation:
Keys may be optionally specified with a type. Untyped values default to strings when queried. The type follows the name separated by a colon:
```scala
pi:float = 3.1415
greeting:str[] = hello|world
```
Supported data types are:
* Basic types:
  * str
  * int
  * float
  * bool
  * date (ISO 8601, "YYYY-MM-DD")
* Lists (items separated by pipes |):
  * str[]
  * int[]
  * float[]

Boolean arrays are currently unsupported. This is a deliberate design choice rather than a technical limitation: without an explicit schema, positional boolean values are typically hard to interpret and offer poor readability. Support may be added if a convincing real-world use case emerges. Similarly, arrays of dates await use cases.

### Lists (arrays)

Values can be expressed in list-format:
```scala
my_array:str[] = foo|bar|baz

# id  arr:int[]
  1   1|2
  2   3|4|5|6|7
```
Lists must always be type annotated or will be treated as strings. Note that the `[]` is a verbatim symbol and does not and cannot take the array length inside the brackets. 

**Arrays are single-dimensional only.** Arf! does not support nested arrays or multi-dimensional array syntax. This design choice maintains:
- Syntactic simplicity (no bracket nesting required)
- Human readability (flat data with clear semantic context)
- Parsing simplicity (single-pass, no depth tracking)

Each array element has semantic context from its containing structure (key name, column header, category). Nested arrays would lose this contextual clarity and go against Arf!'s human-first design philosophy.

### Line comments
```scala
// familiar from C++, Java, C#, etc.
```

### Paragraphs

Free-form prose blocks for documentation, narrative, or explanation:
```scala
This is a paragraph. It can contain any text and spans
until the next structural item. 
 
Paragraphs are first-class entities, tracked and serialized
in document order alongside keys, tables, categories, and comments.
Everything that is not Arf structure will be retained as paragraphs.
```

**Characteristics:**
- Anything not data, comment or structure is prose kept as paragraphs 
- Preserve authored order in serialization
- Can appear anywhere: root, categories, between table rows
- Multiple consecutive paragraph lines form logical blocks

> [!NOTE]
> **Design Philosophy:**
> In most formats, structure is mandatory and prose is an afterthought (if allowed at all). 
> Arf inverts this: prose is the default state, structure is what you opt into. 
> This makes Arf naturally suited for documents where data lives within narrative context, 
> not the other way around.

**Common uses:**
- Documenting configuration sections
- Narrative text in game content
- Explanatory notes in datasets
- Lore and flavor text
- Multi-line descriptions

**Example:**
```scala
weapons:
  Medieval European swords, categorized by battlefield role.
  All weights assume standard steel construction.

  # name            type        weight_kg  damage
    Longsword       two-handed  1.5        1d10
    Arming Sword    one-handed  1.1        1d8

  Historically, the arming sword was the most common knightly 
  weapon, while longswords were specialist weapons for mounted 
  combat and ceremonial use.
/weapons
```

## Categories and hierarchies
The file can be structured into categories and subcategories to form a hierarchy.

Categories are explicitly declared (```name:``` or ```:name```) and closed (```/name```). See below for rules on automatic closing of categories.  

A **top-level category** is a name that ends with a colon:
```scala
settings:
/settings
```

Note that Arf allows for definitions in the root:
```scala
foo = 13
top:
  bar = 42
```
### Subcategories
A **subcategory** is declared by a name *starting* with a colon:
```scala
:graphics
/graphics

:video
/video
```
For brevity, a simple / can be used instead of the full ```/name``` syntax:
```scala
:graphics
/

:video
/
```
### Nesting categories
Subcategories can be nested:
```scala
:settings
  :graphics
  /graphics

  :video
  /video
/settings
```
Multiple subcategories can be closed through explicitly named closures:
```scala
top:
  :sub1
  :sub2
  :sub3
  /sub1
```
### Notes
Closing subcategories:
* The ```/``` without name will only close the most recent subcategory.
* Remember that a new top-level category will reset the nesting:
```scala
top1:
  :sub1
    key_1 = value_1
    :sub2
      key_2 = value_2

// Defining a new top-level category closes the subcategories of top1
top2:
  :sub1
    :sub2
```
More notes:
* Categories are not opened or closed by indentation.
* Categories remain open until explicitly closed or until a new top-level category begins.
* Subcategories must be closed explicitly unless the file ends or a new top-level category is declared.
* Subcategories can only be declared inside other categories. 

## Tables

Tables are declared by a hash (```#```) character as the table designator and the table headers are the names of the tabular data. 
Columns are separated by **two or more spaces**.
```scala
# name    type    value
```
Headers can be type annotated,
```scala
# name:str    type:str    value:int
```
and--just like values--default to string if not typed.

A table is scoped to the category in which its header is defined (the **owning category**). The table remains active while parsing that category and any of its subcategories, and ends when that category is closed or when non-table data is encountered.

Or more formally, a table remains active until one of the following occurs:
* a key–value pair is encountered within the table's owning category or its subcategories,
* a new table header is defined,
* the category in which the table was defined is closed,
* a new top-level category begins.
* or EOF

Example:
```scala
# name    type    value:int
  sword   steel   12
  axe     iron    9
```

### Subcategories in tables
Arf supports subdividing tables without restarting the header.
```scala
items:
#   name      type      value
:weapons
    sword     steel     12
    axe       iron      9
/weapons
:armour
    chain     iron      5
    leather   hide      2
/armour
```

Characteristics:
* Subcategories *participate* in (see and can add rows to) the table. They do not inherit or copy it.
* Subcategories do not restart the table.
* The header applies across all subcategories.
* Each category is closed explicitly.
* Closing a subcategory containing a table (the *owning category*) also closes the table.
* Closing a subcategory started inside the table returns to and continues the previous category in the nesting hierarchy.

Example of a table wrapped in an owning category:
```scala
:header
    # a   b
      1   2
/header
```
Closing header closes the table within it.


> [!NOTE]
> Closing the table's owning category ends the table; closing a participating subcategory resumes the parent scope.

## Practical Demonstration
A larger example showing structure, nested categories, tables, and explicit closure:
```scala
world:
    name = Eldershade
    seed = 1234

:graphics
    resolution = 1920x1080
    fullscreen = false
    gamma = 1.2
/graphics

:entities
    description = List of all active world entities

      # id   type       name
        0    nothing    Nothing
    :creatures
        1    wolf       Greyfang
        2    bear       Old Grizzle
    /creatures
    :humanoids
        10   villager   Maren
        11   guard      Thalk
    /humanoids
        100  boss       Demon Lord
/entities

:structures
    # id    category  durability
  :wood
      100   hut       25
      101   gate      40
  /wood
  :stone
      200   tower     90
      201   keep      150
  /stone
/structures
```

# The Case for Arf!

|   | Pros  | Cons  | Arf! advantages   |
| :--- | :--- | :--- | :--- |
| JSON | ubiquitous, strict | noisy, no comments, no tables | Arf! is easier to read and write, supports comments and tables. |
| YAML | expressive | indentation traps, surprising coercions, many foot-guns | Arf! avoids indentation entirely and keeps rules deterministic. |
| TOML | predictable, well structured | verbose, no table subcategories | Arf! supports structured tables and hierarchical organisation. |
| TOON | clean tables | linear, no hierarchy | Arf! brings TOON's tabular clarity into a hierarchical world. |

## Why Choose Arf!
* **Literate data** Mix prose paragraphs with structured content seamlessly (unique)
* Straightforward hierarchical structure
* Tables with subdivision (rare and extremely useful)
* Minimal and predictable syntax
* Explicit structure via / closure
* Readable by humans, trivial to parse by machines
* Zero indentation rules
* Flexible enough for configs, data definitions, content files, prototypes, world descriptions, resource manifests, etc.

# Non-goals
* No built-in string escaping or interpolation
  * Arf! does not interpret escape sequences or quoted strings.
* No semantic meaning assigned to delimiters
  * Quotes and other characters are treated as ordinary text.
* No whitespace-significant strings
  * Leading whitespace may be trimmed; trailing whitespace is not guaranteed to be preserved.
* No multi-line strings
  * Files remain line-oriented and diff-friendly.
* No implicit typing or coercion
  * Types are explicit or resolved at query time.
* No indentation-based semantics
  * Whitespace is decorative only.
* No schema or validation layer
  * Arf! describes data; correctness is client responsibility.
* No magical inference from empty lines
  * Parsing is driven by syntax, not layout.
* No support for preserving trailing whitespace
  * Invisible state is hostile to humans.
