
var glParameters = {};

function isEmpty(str) {
   return (!str || 0 === str.length);
}

function esc_html(str) {
   return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function esc_regexp(text) {
   return text.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
}

function cancelAnimations() {
   document.getAnimations().forEach((animation) => { animation.cancel(); });
}

const getParentNode = (child, selector) =>
   !selector || !child || !child.parentElement ? undefined
   : (child.parentElement.querySelectorAll(selector).values().find(x => x === child)
   ?? getParentNode(child.parentElement, selector));

// Support function for glParameters

(function () {
   var e,
      a = /\+/g,
      r = /([^&=]+)=?([^&]*)/g,
      d = function (s) { return decodeURIComponent(s.replace(a, " ")); },
      q = window.location.search.substring(1);

   while (e = r.exec(q))
      glParameters[d(e[1])] = d(e[2]);
})();
