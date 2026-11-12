// Atelier's page: the app cards (from atelier.json, the launcher's own catalog) and the year
(function () {
  'use strict';
  const grid = document.getElementById('apps');

  function card(app) {
    const a = document.createElement('article');
    a.className = 'app';
    const img = document.createElement('img');
    img.src = app.logo || 'site/logo.png';
    img.alt = app.name;
    img.onerror = function () { img.src = 'site/logo.png'; };
    const body = document.createElement('div');
    const h = document.createElement('h2');
    const link = document.createElement('a');
    link.href = 'https://github.com/' + (app.repo || 'CursedPrograms/' + app.name);
    link.textContent = app.name;
    h.appendChild(link);
    const p = document.createElement('p');
    p.textContent = app.description || '';
    body.appendChild(h);
    body.appendChild(p);
    if (app.plugin) {
      const n = document.createElement('p');
      n.className = 'note';
      n.textContent = app.plugin;
      body.appendChild(n);
    }
    a.appendChild(img);
    a.appendChild(body);
    return a;
  }

  if (grid) {
    fetch('atelier.json')
      .then(function (r) { return r.json(); })
      .then(function (data) { (data.apps || []).forEach(function (app) { grid.appendChild(card(app)); }); })
      .catch(function () { grid.textContent = 'The apps are listed on GitHub.'; });
  }

  const year = document.getElementById('year');
  if (year) year.textContent = String(new Date().getFullYear());
})();
