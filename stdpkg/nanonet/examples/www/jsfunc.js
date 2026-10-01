
function JsFuncFillDiv(adivname)
{
  var div = document.getElementById(adivname);
  if (!div) return;

  div.innerHTML = '<h3>This text comes from javascript / 4</h3>';
}