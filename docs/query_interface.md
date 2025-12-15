# Querying data in an Arf! document

```include "arf_parser.hpp"```

## Parsing an Arf! document

```arf::parse``` operates on string data and returns the document tree:
```
std::string example_config = obtain_arf_data_from_somewhere();

auto result = arf::parse(example_config);    
if (result.has_value())
{
    // result->doc contains the parsed document
}
if (result.has_errors())
{
    // result.errors contains the errors emitted when parsing
}
```
## Note on data types

Arf! uses the names "float" and "int" to annotate value types. Internally these are stored as ```double``` and ```int64_t```; the "float" and "int" names are convenience types.

## Paths and structured queries

Basic data (key/value pairs) are accessed through string paths. 
Arrays and tables are accessed through structured APIs and are not addressable via string paths.

## Querying key\value data

Key/value data queries are **path-centric** and accessed by dot-separated path, where the last segment is the key (the name of the value). If the path resolves to a category, table, or anything non-scalar, the query returns std::nullopt without error.

```
// foo is declared in the root:
foo = 13

bar:
// baz is located under the category bar:
    baz = 42
```
* The path to foo is: ```foo```
* The path to baz is: ```bar.baz```

Accessor functions all take the document and a path as arguments and returns a std::optional<T>:
```
  std::optional<float> volume = arf::get_float(doc, "game_settings.audio.master_volume");
```
* ```arf::get_string``` -> ```std::string```
* ```arf::get_int``` -> ```int```
* ```arf::get_bool``` -> ```bool```
* ```arf::get_float``` -> ```float```

If the value at the path is of the requested type it is returned as such, otherwise a conversion is attempted. Failures result in ```std::nullopt```.

## Reflection
```
std::optional<value_ref> get(const document& doc, const std::string& path);
```
returns a ```value_ref``` that allows the client to reflect over the content of the document (names, types, structure). The ```value_ref``` object provides the following operations:

```
bool is_scalar()
bool is_array()         
bool is_string() 
bool is_int() 
bool is_float() 
bool is_bool()          
bool is_string_array() 
bool is_int_array() 
bool is_float_array()

std::optional<std::string> as_string()
std::optional<int64_t> as_int()
std::optional<double> as_float()
std::optional<bool> as_bool()
template<typename T> std::optional<std::span<const T>> as_array()
const value & raw() const
```        

```value_ref``` is intended for reflective and tooling use. Application code should prefer typed accessors (```get_int```, ```get_string```, etc.) whenever possible.

## Querying tables

Table queries are **structure-centric** and the table tree is navigated explicitly. Given the table
```
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
```
table columns are enumerated as
```
auto & monsters = doc.categories["monsters"];

for (auto const & [col_name, col_type] : monsters->table_columns)
    std::cout << std::format("{}: {}\n", col_name, arf::detail::type_to_string(col_type));
```
and table rows are enumerated as
```
auto & monsters = doc.categories["monsters"];

// Base rows
for (auto & row : monsters->table_rows) { ... }

// Subcategory rows
for (auto & [name, subcat] : monsters->subcategories)
    for (auto & subrow : subcat->table_rows) { ... }
```
The values are obtained through ```std::get<T>(cell)```
```
auto id = std::get<int>(row[0]);
auto name = std::get<std::string>(row[1]);
auto count = std::get<int64_t>(row[2]);
```
Note that sub-rows (table rows under table subcategories) are not enumerated as part of the higher category and must be manually collected from the subcategory.

## Querying arrays

Arrays are stored as typed std::vector<T> values and must be extracted using ```std::get```. 

From key/values:
```
auto v = get_value(doc, "player.tags");
auto const & tags = std::get<std::vector<std::string>>(*v);
```
and from a table row,
```
auto const & skills = std::get<std::vector<std::string>>(row[5]);

```
