(() => {
  const $ = id => document.getElementById(id);
  // Development adapter only: replace with authenticated account APIs in production.
  const key = 'aquasense.mock.profile';
  const demoAccount = { name: 'Jamie Dela Cruz', email: 'jamie.delacruz@email.com', password: 'AquaSense123!' };
  const account = { ...demoAccount };
  try { const saved = JSON.parse(localStorage.getItem(key)); if (saved && typeof saved.name === 'string' && typeof saved.email === 'string') Object.assign(account, { name: saved.name, email: saved.email }); } catch {}
  let signedIn = sessionStorage.getItem('aquasense.mock.signedOut') !== 'true';
  let editing = false, returnFocus;
  const normalize = value => value.trim().replace(/\s+/g, ' ');
  const initials = name => name.split(/\s+/).slice(0, 2).map(word => Array.from(word)[0]).join('').toUpperCase();
  const render = () => {
    $('account-name').textContent = account.name;
    $('account-email').textContent = account.email;
    $('account-initials').textContent = initials(account.name);
    $('profile-button').querySelector('.avatar').textContent = initials(account.name);
    $('profile-button').querySelector('strong').textContent = account.name;
    $('profile-button').setAttribute('aria-label', `${account.name}, Aquarium Owner`);
    const hour = new Date().getHours();
    $('greeting').textContent = `${hour < 12 ? 'Good morning' : hour < 18 ? 'Good afternoon' : 'Good evening'}, ${account.name.split(' ')[0]}.`;
    if (!editing) { $('profile-name').value = account.name; $('profile-email').value = account.email; }
  };
  const setEditing = value => {
    editing = value;
    for (const field of ['name', 'email']) { $('profile-' + field).readOnly = !value; $('profile-' + field).removeAttribute('aria-invalid'); $(field + '-error').textContent = ''; }
    $('edit-profile').hidden = value; $('save-profile').hidden = !value; $('cancel-profile').hidden = !value;
    render();
    (value ? $('profile-name') : $('edit-profile')).focus();
  };
  $('edit-profile').onclick = () => { $('account-message').textContent = ''; setEditing(true); };
  $('cancel-profile').onclick = () => setEditing(false);
  $('profile-form').onsubmit = event => {
    event.preventDefault(); if (!editing) return;
    const name = normalize($('profile-name').value), email = $('profile-email').value.trim();
    const errors = { name: name ? '' : 'Name is required.', email: !email ? 'Email is required.' : /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) ? '' : 'Enter a valid email address.' };
    for (const field of ['name', 'email']) { $(field + '-error').textContent = errors[field]; $('profile-' + field).setAttribute('aria-invalid', String(Boolean(errors[field]))); }
    const invalid = Object.keys(errors).find(field => errors[field]);
    if (invalid) { $('profile-' + invalid).focus(); return; }
    try { localStorage.setItem(key, JSON.stringify({ name, email })); } catch { $('account-message').textContent = 'Unable to save your profile. Please try again.'; return; }
    Object.assign(account, { name, email }); setEditing(false); $('account-message').textContent = 'Profile updated successfully.';
  };
  const close = () => {
    $('account-modal').classList.remove('open'); $('account-modal').hidden = true; $('password-form').reset();
    document.querySelectorAll('.password-error').forEach(el => el.textContent = '');
    document.querySelectorAll('.password-field').forEach(field => {
      const input = field.querySelector('input'), button = field.querySelector('button');
      input.type = 'password'; input.removeAttribute('aria-invalid'); button.textContent = 'Show';
      button.setAttribute('aria-pressed', 'false'); button.setAttribute('aria-label', `Show ${field.querySelector('label').textContent.toLowerCase()}`);
    });
    returnFocus?.focus();
  };
  const open = logout => {
    if (!signedIn) return;
    returnFocus = document.activeElement;
    $('account-modal-title').textContent = logout ? 'Log out?' : 'Change Password';
    $('password-form').hidden = logout; $('logout-confirm').hidden = !logout;
    $('account-modal').hidden = false; $('account-modal').classList.add('open');
    (logout ? $('logout-cancel') : $('current-password')).focus();
  };
  for (const [id, label, autocomplete] of [['current', 'Current Password', 'current-password'], ['new', 'New Password', 'new-password'], ['confirm', 'Confirm New Password', 'new-password']]) {
    const wrapper = document.createElement('div'); wrapper.className = 'password-field';
    wrapper.innerHTML = `<label for="${id}-password">${label}</label><div class="password-input"><input id="${id}-password" type="password" autocomplete="${autocomplete}" aria-describedby="${id}-password-error"><button type="button" aria-label="Show ${label.toLowerCase()}" aria-pressed="false">Show</button></div><small class="account-error password-error" id="${id}-password-error"></small>`;
    wrapper.querySelector('button').onclick = event => { const input = wrapper.querySelector('input'); const visible = input.type === 'password'; input.type = visible ? 'text' : 'password'; event.currentTarget.textContent = visible ? 'Hide' : 'Show'; event.currentTarget.setAttribute('aria-label', `${visible ? 'Hide' : 'Show'} ${label.toLowerCase()}`); event.currentTarget.setAttribute('aria-pressed', String(visible)); };
    $('password-fields').append(wrapper);
  }
  const hint = document.createElement('p'); hint.textContent = 'Use at least 8 characters, including uppercase and lowercase letters and a number.'; $('password-fields').append(hint);
  $('change-password').onclick = () => open(false);
  $('password-cancel').onclick = close; $('logout-cancel').onclick = close;
  $('password-form').onsubmit = event => {
    event.preventDefault();
    const current = $('current-password').value, next = $('new-password').value, confirm = $('confirm-password').value;
    const errors = { current: current !== account.password ? 'Current password is incorrect.' : '', new: next === current ? 'New password must differ from the current password.' : !/^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,}$/.test(next) ? 'Use at least 8 characters, uppercase and lowercase letters, and a number.' : '', confirm: next !== confirm ? 'New passwords do not match.' : !confirm ? 'Confirm your new password.' : '' };
    for (const id of Object.keys(errors)) { $(id + '-password-error').textContent = errors[id]; $(id + '-password').setAttribute('aria-invalid', String(Boolean(errors[id]))); }
    const invalid = Object.keys(errors).find(id => errors[id]); if (invalid) { $(invalid + '-password').focus(); return; }
    account.password = next; close(); $('account-message').textContent = 'Password changed successfully.';
  };
  $('logout-confirm-button').onclick = () => { sessionStorage.setItem('aquasense.mock.signedOut', 'true'); signedIn = false; setEditing(false); close(); $('login-form').reset(); location.hash = '#login'; };
  $('use-demo-login').onclick = () => {
    $('login-email').value = demoAccount.email;
    $('login-password').value = demoAccount.password;
    $('login-error').textContent = '';
    $('login-form').requestSubmit();
  };
  $('login-form').onsubmit = event => {
    event.preventDefault();
    const email = $('login-email').value.trim().toLowerCase();
    const password = $('login-password').value.trim();
    const matchesCurrent = email === account.email.toLowerCase() && password === account.password;
    const matchesDemo = email === demoAccount.email.toLowerCase() && password === demoAccount.password;
    if (!matchesCurrent && !matchesDemo) {
      $('login-error').textContent = 'Use jamie.delacruz@email.com and AquaSense123!, or click Use demo account.';
      return;
    }
    sessionStorage.removeItem('aquasense.mock.signedOut');
    signedIn = true;
    $('login-error').textContent = '';
    $('login-form').reset();
    location.hash = '#dashboard';
  };
  $('account-modal').onclick = event => { if (event.target === $('account-modal')) close(); };
  document.addEventListener('keydown', event => {
    if (!$('account-modal').classList.contains('open')) return;
    if (event.key === 'Escape') close();
    if (event.key === 'Tab') { const items = [...$('account-modal').querySelectorAll('input, button')].filter(el => el.getClientRects().length); const first = items[0], last = items.at(-1); if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last.focus(); } else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first.focus(); } }
  });
  window.aquaSenseAccount = { render, isSignedIn: () => signedIn, confirmLogout: () => open(true) };
  render();
})();
