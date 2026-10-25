'use strict';

// Keep existing console bookmarks and OAuth returns working.
function openConsoleBookmark() {
  if (/^#(?:overview|planner|connections|activity|token=)/.test(location.hash)) {
    location.replace(`/console.html${location.search}${location.hash}`);
  }
}
openConsoleBookmark();
window.addEventListener('hashchange', openConsoleBookmark);
