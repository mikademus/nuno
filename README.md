# Arf! — A Readable Format

| **A tiny, human-centred data language for hierarchical configs and tables** |
|-----------------------------------------------------------------------------|

Arf! (“A Readable Format”, also the bark of an enthusiastic small dog) is a compact, predictable, deterministic and explicitly scoped data language built for human readability without giving up structure. It mixes hierarchical categories, key/value pairs, and TOOL-style tables (column-aligned, whitespace-delimited) that can be subdivided into named subsections.

The goals are simple:

* Allow key/value and tabular data
* Minimal syntax, predictable parsing, indentation-independent structure
* Be an improvement over leading alternatives:
  * Easier to write and read than JSON
  * Less fragile and surprising than YAML
  * More flexible than TOML
  * Much more expressive than plain TOOL tables

## Overview

Arf! provides:

* Hierarchical categories using a minimal marker syntax rather than indentation
* Key/value bindings with optional type annotation
* Tables with TOOL-like clarity
* Table subcategories that do not restart headers
* Explicit category closing for precision and control
* Whitespace freedom outside tables
* Fully deterministic parsing rules

Arf! aims to be the practical everyday format for configs, content files, world definitions, and structured datasets.

## Quick Example

```
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

## Rationale

* JSON is rigid and noisy.
* YAML is permissive to the point of being a riddle.
* TOML is reliable but structurally repetitive.
* TOOL tables are lovely but limited.

Arf! attempts to unify the strengths of each:
* JSON’s predictability
* YAML’s readability
* TOML’s clarity
* TOOL’s table ergonomics
…while discarding their pain points.

# Syntax Overview
## Basic constructs
### Whitespace
Outside tables whitespace is purely cosmetic for the benefit of human readers. Writers are encouraged to use indentation, empty lines and comments to structure their files for the sake of presentation. Inside tables cells are separated by **two or more spaces**.

### Key/Value Pairs
```
key = value
```
String values
* Strings may contain spaces, and multiple consecutive spaces are allowed.
* Strings are not delimited by quotation marks and end at EOL.
* There is no support for multi-line strings

### Type annotation:
Keys may be optionally specified with a type. Untyped values default to strings when queried. The type follows the name separated by a colon:
```
pi:float = 3.1415
greeting:str[] = hello|world
```
Supported data types are:
* Basic types:
  * str
  * float
  * bool
  * date (ISO 8601, "YYYY-MM-DD")
* Lists (items separated by pipes |):
  * str[]
  * int[]
  * float[]

### Line comments
```
// familiar from C++, Java, C#, etc.
```

## Categories and hierarchies
The file can be structured into categories and subcategories to form a hierarchy.

Categories are explicitly declared (```name:``` or ```:name```) and closed (```/name```). See below for rules on automatic closing of categories.  

A **top-level category** is a name that ends with a colon:
```
settings:
graphics:
```
Top-level categories always originate from the root and reset any existing nesting of subcategories, thus they do not need to but may be explicitly closed.

Note that Arf allows for definitions in the root:
```
foo = 13
top:
  bar = 42
```
### Subcategories
A **subcategory** is declared by a name *starting* with a colon:
```
:graphics
/graphics

:video
/video
```
For brevity, a simple / can be used instead of the full ```/name``` syntax:
```
:graphics
/

:video
/
```
### Nesting categories
Subcategories can be nested:
```
:settings
  :graphics
  /grapics

  :video
  /video
/settings
```
Multiple subcategories can be closed through explicitly named closures:
```
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
```
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

Tables are declared by by a hash (```#```) character as the table designator and the table headers are the names of the tabular data. 
Columns are separated by **two or more spaces**.
```
# name    type    value
```
Headers can be type annotated,
```
# name:str    type:str    value:int
```
and like values default to string if not typed.

A table is scoped to the category in which its header is defined. The table remains active while parsing that category and any of its subcategories, and ends when that category is closed or when non-table data is encountered.

Or more formally, a table remains active until one of the following occurs:
* a key–value pair is encountered within the table’s owning category or its subcategories,
* a new table header is defined,
* the category in which the table was defined is closed,
* or a new top-level category begins.

Example:
```
# name    type    value:int
  sword   steel   12
  axe     iron    9
```

### Subcategories in tables
Arf supports subdividing tables without restarting the header.
```
items:
#   name   type   value
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
* subcategories do not restart the table
* header applies across all subcategories
* each category is closed explicitly
* closing a subcategory containing a table also closes the table

Example of a table wrapped in a category:
```
:header
    # a   b
      1   2
/header
```
Closing header closes the table within it.

Closing a subcategory returns to and continues the previous category in the nesting hierarchy. 

## Practical Demonstration
A larger example showing structure, nested categories, tables, and explicit closure:
```
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
## How Arf! Compares
### JSON
* **Pros**: ubiquitous, strict
* **Cons**: noisy, no comments, no tables

Arf! is easier to read and write, supports comments and tables.

#### YAML
* **Pros**: expressive
* **Cons**: indentation traps, surprising coercions, many foot-guns

Arf! avoids indentation entirely and keeps rules deterministic.

#### TOML
* **Pros**: predictable, well structured
* **Cons**: verbose, no table subcategories

Arf! supports structured tables and hierarchical organisation.

#### TOOL
* **Pros**: clean tables
* **Cons**: linear, no hierarchy

Arf! brings TOOL’s tabular clarity into a hierarchical world.

## Why Choose Arf!
* Straightforward hierarchical structure
* Tables with subdivision (rare and extremely useful)
* Minimal and predictable syntax
* Explicit structure via / closure
* Readable by humans, trivial to parse by machines
* Zero indentation rules
* Flexible enough for configs, data definitions, content files, prototypes, world descriptions, resource manifests, etc.
