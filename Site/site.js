/* Translation data is compiled locally by Tools/build_site.py. */
const translations = /*__TRANSLATIONS__*/ {};
const query = new URL(location.href);
let language = query.searchParams.get('lang') === 'fr' ? 'fr' : 'en';
const dialog = document.querySelector('#lightbox');
const gallery = [...document.querySelectorAll('[data-lightbox]')];
let galleryIndex = 0;
let galleryOpener = null;
const menu = document.querySelector('#primary-nav');
const menuButton = document.querySelector('.menu-toggle');
const t = key => translations[language][key];

// Keep former README/documentation root routes working when the showcase is
// eventually served at the GitHub Pages root. The actual reader is unchanged.
if (/^#\/(en|fr)\//.test(location.hash)) {
  const reader = new URL('Documentation.html', location.href);
  reader.hash = location.hash;
  reader.search = location.search;
  location.replace(reader.href);
}

function displayImage(index) {
  galleryIndex = (index + gallery.length) % gallery.length;
  const button = gallery[galleryIndex];
  const image = document.querySelector('#lightbox-image');
  image.src = button.querySelector('img').src;
  image.alt = t(button.dataset.caption);
  document.querySelector('#lightbox-caption').textContent = image.alt;
}

function closeMenu() {
  menu.classList.remove('is-open');
  menuButton.setAttribute('aria-expanded', 'false');
  menuButton.setAttribute('aria-label', t('menu.open'));
}

function applyLanguage(next, updateUrl = false) {
  language = next === 'fr' ? 'fr' : 'en';
  document.documentElement.lang = language;
  document.title = t('meta.title');
  document.querySelector('meta[name=description]').content = t('meta.description');
  document.querySelectorAll('[data-i18n]').forEach(node => { node.textContent = t(node.dataset.i18n); });
  document.querySelectorAll('[data-i18n-alt]').forEach(node => { node.alt = t(node.dataset.i18nAlt); });
  document.querySelectorAll('[data-i18n-aria]').forEach(node => { node.setAttribute('aria-label', t(node.dataset.i18nAria)); });
  document.querySelectorAll('[data-language]').forEach(node => { node.setAttribute('aria-pressed', String(node.dataset.language === language)); });
  document.querySelectorAll('[data-doc]').forEach(link => { link.href = 'Documentation.html#/' + language + '/' + link.dataset.doc; });
  gallery.forEach(button => { button.setAttribute('aria-label', t(button.dataset.caption)); });
  menuButton.setAttribute('aria-label', t(menu.classList.contains('is-open') ? 'menu.close' : 'menu.open'));
  if (dialog.open) displayImage(galleryIndex);
  if (updateUrl) {
    const url = new URL(location.href);
    url.searchParams.set('lang', language);
    history.replaceState(null, '', url);
  }
}

document.querySelectorAll('[data-language]').forEach(button => {
  button.addEventListener('click', () => applyLanguage(button.dataset.language, true));
});
menuButton.addEventListener('click', () => {
  const open = menu.classList.toggle('is-open');
  menuButton.setAttribute('aria-expanded', String(open));
  menuButton.setAttribute('aria-label', t(open ? 'menu.close' : 'menu.open'));
});
menu.querySelectorAll('a').forEach(link => link.addEventListener('click', closeMenu));
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') closeMenu();
  if (dialog.open && ['ArrowLeft', 'ArrowRight'].includes(event.key)) {
    event.preventDefault();
    displayImage(galleryIndex + (event.key === 'ArrowRight' ? 1 : -1));
  }
});
gallery.forEach((button, index) => button.addEventListener('click', () => {
  galleryOpener = button;
  displayImage(index);
  dialog.showModal();
}));
document.querySelector('.lightbox-close').addEventListener('click', () => dialog.close());
document.querySelectorAll('[data-gallery]').forEach(button => button.addEventListener('click', () => displayImage(galleryIndex + Number(button.dataset.gallery))));
dialog.addEventListener('click', event => {
  if (event.target === dialog) {
    const bounds = dialog.getBoundingClientRect();
    if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) dialog.close();
  }
});
dialog.addEventListener('close', () => galleryOpener?.focus({preventScroll:true}));
const chapters = [...document.querySelectorAll('.chapter-links a')];
if ('IntersectionObserver' in window) {
  const observer = new IntersectionObserver(entries => {
    for (const entry of entries) {
      if (!entry.isIntersecting) continue;
      chapters.forEach(link => {
        if (link.hash === '#' + entry.target.id) link.setAttribute('aria-current', 'location');
        else link.removeAttribute('aria-current');
      });
    }
  }, {rootMargin:'-15% 0px -45% 0px'});
  chapters.forEach(link => { const section = document.querySelector(link.hash); if (section) observer.observe(section); });
}
applyLanguage(language);
