# Arf! — A Readable Format

| **A tiny, human-centred data language for hierarchical configs and tables;<br> Readable by humans, trivial to parse by machines** | ![mascot](arf_mascot_small.png) |
|-------------------------------------------------------------------------------------------------------------------------------|---------------------------------------|

Arf! (“A Readable Format”, also the bark of an enthusiastic small dog) is a compact, predictable, deterministic and explicitly scoped data language built for human readability without giving up structure. It mixes hierarchical categories, key/value pairs, and TOON-style tables[^1] (column-aligned, whitespace-delimited) that can be subdivided into named subsections.

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
| Comments	               | ❌	   | ✅	   | ✅	   | ⚠️    | ✅    | TOON: By convention, not by specification  |
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

## Philosophy: The Human-First Protocol, Built for Speed

Most modern data formats are designed solely for transmission (JSON) or system-level automation (YAML). They prioritize rigid schemas or opaque parsing rules. Arf! is built on the belief that for many tasks—configuration, world-building, and creative data entry—clarity for the human writer can co-exist with trivial parsing for the machine.

1. **Readability is Not a "Feature"; It’s the Goal**<br />
In Arf!, you should be able to scan a file and understand its hierarchy through visual landmarks (colons and slashes) rather than counting invisible whitespace or matching nested braces. If you can’t read it comfortably, it’s a chore.
2. **The "Copy-Paste" Safety Principle**<br />
One of the greatest frustrations with indentation-based formats (like YAML or TOON) is that moving a block of text can silently change its meaning or break the parser. Arf! uses explicit scoping that preserves structural integrity no matter where you paste the data.
3. **Determinism Over "Magic"**<br />
Arf! avoids "magic" parsing. In YAML, the string NO might be converted to a boolean false automatically. In Arf!, a string is a string. We believe the computer should never have to guess what the human meant. This deterministic structure is precisely what allows for fast, simple machine parsing.
4. **Tables are First-Class Citizens**<br />
Data isn't just trees; it’s often rows. Forcing tabular data into nested Key/Value pairs (JSON) or repetitive arrays (TOML) is an ergonomic failure. Arf! treats the Table as a primary construct, allowing humans to maintain the "spreadsheet view" they naturally prefer for lists of entities.
5. **Graceful Degradation**<br />
Arf! is designed to be "hand-rolled." You shouldn't need a specialized IDE plugin to write a config file. By allowing whitespace freedom outside of tables and offering simple shorthand closures (/), Arf! scales easily.

While other formats create complex problems that require complex parsers, Arf! provides elegant simplicity for both the human author and the machine reader.

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
* TOON tables are lovely but limited.

Arf! attempts to unify the strengths of each:
* JSON’s predictability
* YAML’s readability
* TOML’s clarity
* TOON’s table ergonomics
…while discarding their pain points.

## Note on 0.2.0

> [!WARNING]
> Arf! is divided into three areas of resposibility: parsing, querying and serialisation. Serialisation is intended to be document-ordered and retain the order and structure of a parsed document. Note that **in this version** the serialiser _normalises data to canonical order_. This will be amended.

# Syntax Overview
## Basic constructs
### Whitespace
Outside tables whitespace is purely cosmetic for the benefit of human readers. Writers are encouraged to use indentation, empty lines and comments to structure their files for the sake of presentation. Inside tables cells are separated by **two or more spaces**.

### Key/Value Pairs
```
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
```
path = C:\Games\Foo\bar\n
text = "   padded string   "
regex = ^\w+\s+\w+$
``` 

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

Boolean arrays are currently unsupported. This is a deliberate design choice rather than a technical limitation: without an explicit schema, positional boolean values are typically hard to interpret and offer poor readability. Support may be added if a convincing real-world use case emerges. Similarly, arrays of dates await use cases.

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
  /graphics

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

Tables are declared by a hash (```#```) character as the table designator and the table headers are the names of the tabular data. 
Columns are separated by **two or more spaces**.
```
# name    type    value
```
Headers can be type annotated,
```
# name:str    type:str    value:int
```
and--just like values--default to string if not typed.

A table is scoped to the category in which its header is defined (the **owning category**). The table remains active while parsing that category and any of its subcategories, and ends when that category is closed or when non-table data is encountered.

Or more formally, a table remains active until one of the following occurs:
* a key–value pair is encountered within the table’s owning category or its subcategories,
* a new table header is defined,
* the category in which the table was defined is closed,
* a new top-level category begins.
* or EOF

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
```
:header
    # a   b
      1   2
/header
```
Closing header closes the table within it.


> [!NOTE]
> Closing the table’s owning category ends the table; closing a participating subcategory resumes the parent scope.

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

|   | Pros  | Cons  | Arf! advantages   |
| :--- | :--- | :--- | :--- |
| JSON | ubiquitous, strict | noisy, no comments, no tables | Arf! is easier to read and write, supports comments and tables. |
| YAML | expressive | indentation traps, surprising coercions, many foot-guns | Arf! avoids indentation entirely and keeps rules deterministic. |
| TOML | predictable, well structured | verbose, no table subcategories | Arf! supports structured tables and hierarchical organisation. |
| TOON | clean tables | linear, no hierarchy | Arf! brings TOON’s tabular clarity into a hierarchical world. |

## Why Choose Arf!
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
