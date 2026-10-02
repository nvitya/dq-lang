const test = require("node:test");
const assert = require("node:assert/strict");
const { blockCloserForHeader, hasIndentedBodyLine } = require("../block-closer");

test("finds closers for DQ block headers", () => {
  const cases = [
    ["if value < 0:", "endif"],
    ["for item in items:", "endfor"],
    ["while running:", "endwhile"],
    ["func main() -> int:", "endfunc"],
    ["object Counter:", "endobject"],
    ["struct Point:", "endstruct"]
  ];

  for (const [header, closer] of cases) {
    assert.deepEqual(blockCloserForHeader(`  ${header}`), {
      indentation: "  ",
      text: closer
    });
  }
});

test("ignores non-header lines", () => {
  assert.equal(blockCloserForHeader("if value < 0"), undefined);
  assert.equal(blockCloserForHeader("value: int"), undefined);
  assert.equal(blockCloserForHeader("endif"), undefined);
});

test("recognizes an existing body line immediately after a new blank line", () => {
  assert.equal(hasIndentedBodyLine([
    "            ",
    "            var jrow :? = jdata.AddArr('')"
  ], "        "), true);
});

test("does not treat blank or equally indented lines as a body", () => {
  assert.equal(hasIndentedBodyLine([
    "            ",
    "        endwhile"
  ], "        "), false);
});
