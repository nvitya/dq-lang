const blockClosers = {
  if: "endif",
  for: "endfor",
  while: "endwhile",
  func: "endfunc",
  object: "endobject",
  struct: "endstruct"
};

function blockCloserForHeader(line) {
  const match = /^(\s*)(if|for|while|func|object|struct)\b.*:\s*$/.exec(line);
  if (!match) return undefined;

  return {
    indentation: match[1],
    text: blockClosers[match[2]]
  };
}

module.exports = { blockCloserForHeader };
