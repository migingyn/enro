const film = document.querySelector('video');
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
// The native player remains available when autoplay is disabled or blocked.
if (!reducedMotion.matches && !navigator.connection?.saveData) {
  film.play().catch(() => {});
}
reducedMotion.addEventListener('change', event => {
  if (event.matches) film.pause();
});
