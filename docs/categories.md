# Current format

* `top_category:`
* `:subcategory` - nests under the currently open cateogry (push)
* `/subcategory` - explicitly closes a subcategory
* `/` - closes the current subcategory (pop)

# New and extended format

Top categories have trailing colons. The same.
* `top:` - top category

Subategories are colon-delimited paths:
1. `:rho` - nest under the top category
2. `:foo` - nest under the top category, sibling to and closing `rho`
3. `:foo:bar` - nest under foo
4. `:foo:bar:baz` - explicit path
5. `:foo:bar` - returns to `foo:bar`, closing `foo:bar:baz`. Multiple levels can be closed immediately.
6. `:phi` - starts a new subcategory under the top category, closing all other open subcategories

## Shorthand

1. `:foo` - first subcategory
2. `::bar` - nests under the current subcategory, creating `:foo:bar`.
3. `:::baz` - creates `foo:bar:baz`.
4. `:phi` - closes all subcategories until nearest sibling (`foo`).

## Full format

Concept (rehash):
* Top categories end with colon, indicating "structure from here".
* Subcategories begin with colon, indicating "there is content from here".
* This leads naturally to a `top:sub1:sub2` syntax, giving meaning to 
  * the colon notation ("put it together and it is a path")
  * the colon stacking ("we can leave out the repeated segments")
* Conceivably, the full form could be allowed as a general syntax.

## The old syntax

* There is no reason to remove /-closing.
* There are reasons to explicitly close sobcategories, and to close named subcategories
* The only way to return to the top level (f.i. to make late rows) is to explicitly close the penultimate level.
