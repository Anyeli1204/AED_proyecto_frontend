const API_BASE_URL = 'http://localhost:8080';

let currentToken = localStorage.getItem('sessionToken') || null;
let currentEmail = localStorage.getItem('sessionEmail') || null;

document.addEventListener('DOMContentLoaded', () => {
    initializeApp();
    setupEventListeners();
});

function initializeApp() {
    if (currentToken) {
        document.getElementById('servicioToken').value = currentToken;
        document.getElementById('logoutToken').value = currentToken;
        validateTokenOnLoad();
    }
}

async function validateTokenOnLoad() {
    if (!currentToken) return;
    
    try {
        const response = await fetch(`${API_BASE_URL}/servicio?token=${encodeURIComponent(currentToken)}`, {
            method: 'GET',
        });
        
        const data = await response.json();
        
        if (!response.ok || data.mensaje !== 'Acceso permitido') {
            console.log('[FRONTEND] Token expirado detectado al cargar página, limpiando sesión local');
            clearLocalSession();
        }
    } catch (error) {
        console.error('[FRONTEND] Error al validar token al cargar:', error);
    }
}

function clearLocalSession() {
    currentToken = null;
    currentEmail = null;
    localStorage.removeItem('sessionToken');
    localStorage.removeItem('sessionEmail');
    document.getElementById('servicioToken').value = '';
    document.getElementById('logoutToken').value = '';
}

function setupEventListeners() {
    // Login
    document.getElementById('loginForm').addEventListener('submit', handleLogin);
    
    // Servicio (Validar Sesión)
    document.getElementById('servicioForm').addEventListener('submit', handleServicio);
    
    // Logout
    document.getElementById('logoutForm').addEventListener('submit', handleLogout);
    
    // Admin
    document.getElementById('adminForm').addEventListener('submit', handleAdminClear);
}

async function switchTab(tabName) {
    // Si es la pestaña de admin, verificar token primero
    if (tabName === 'admin') {
        const canAccess = await validateTokenForAdmin();
        if (!canAccess) {
            return; // No cambiar de pestaña si no puede acceder
        }
    }
    
    // Ocultar todas las pestañas
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Desactivar todos los botones
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    // Mostrar la pestaña seleccionada
    document.getElementById(`tab-${tabName}`).classList.add('active');
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
}

async function validateTokenForAdmin() {
    const token = currentToken || localStorage.getItem('sessionToken');
    
    if (!token) {
        showAlert('❌ No hay sesión activa. Por favor, inicia sesión primero.', 'error');
        return false;
    }
    
    try {
        const responseDiv = document.getElementById('adminResponse');
        responseDiv.innerHTML = createSimpleCard('⏳', 'Verificando', 'Verificando token activo con el servidor...');
        
        switchTabWithoutValidation('admin');
        
        const response = await fetch(`${API_BASE_URL}/servicio?token=${encodeURIComponent(token)}`, {
            method: 'GET',
        });
        
        const data = await response.json();
        
        if (response.ok && data.mensaje === 'Acceso permitido') {
            currentToken = token;
            if (localStorage.getItem('sessionToken') !== token) {
                localStorage.setItem('sessionToken', token);
            }
            responseDiv.innerHTML = createSimpleCard('✅', 'Estado', 'Token válido. Puedes usar las funciones de administración.');
            return true;
        } else {
            showAdminAccessError(`Token no válido o expirado: ${data.mensaje || 'Sesión no autorizada'}`);
            clearLocalSession();
            return false;
        }
    } catch (error) {
        showAdminAccessError(`❌ Error al verificar token con el servidor: ${error.message}`);
        return false;
    }
}

// Función para mostrar error de acceso a admin
function showAdminAccessError(message) {
    // Mostrar en el panel de respuesta de admin con tarjeta simple
    const responseDiv = document.getElementById('adminResponse');
    responseDiv.innerHTML = createSimpleCard('❌', 'Error', message + '\n\nPor favor, ve a la pestaña de Login e inicia sesión primero.');
    
    // Mostrar alerta
    showAlert(message + '\n\nSerás redirigido a la pestaña de Login.', 'error');
    
    // Cambiar a la pestaña de login después de mostrar el error
    setTimeout(() => {
        switchTabWithoutValidation('login');
    }, 1000);
}

function createSimpleCard(icon, label, value) {
    return `
        <div class="response-card-simple">
            <div class="card-icon-simple">${icon}</div>
            <div class="card-content-simple">
                <div class="card-label-simple">${label}</div>
                <div class="card-value-simple">${escapeHtml(value)}</div>
            </div>
        </div>
    `;
}

function switchTabWithoutValidation(tabName) {
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    document.getElementById(`tab-${tabName}`).classList.add('active');
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
}

// ==================== LOGIN ====================
async function handleLogin(e) {
    e.preventDefault();
    
    const correo = document.getElementById('correo').value;
    const password = document.getElementById('password').value;
    const responseDiv = document.getElementById('loginResponse');
    
    try {
        responseDiv.innerHTML = '<p style="color: #f59e0b;">⏳ Enviando petición...</p>';
        
        const startTime = Date.now();
        const response = await fetch(`${API_BASE_URL}/login`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ correo, password }),
        });
        
        const endTime = Date.now();
        const duration = endTime - startTime;
        const data = await response.json();
        
        // Guardar token si el login fue exitoso
        if (response.ok && data.token) {
            currentToken = data.token;
            currentEmail = correo;
            localStorage.setItem('sessionToken', currentToken);
            localStorage.setItem('sessionEmail', currentEmail);
            
            // Actualizar tokens en otros formularios
            document.getElementById('servicioToken').value = currentToken;
            document.getElementById('logoutToken').value = currentToken;
            
            showResponse(responseDiv, response.status, data, duration, 'success');
        } else {
            showResponse(responseDiv, response.status, data, duration, 'error');
        }
    } catch (error) {
        showResponse(responseDiv, 0, { error: error.message }, 0, 'error');
    }
}

// ==================== VALIDAR SESIÓN ====================
async function handleServicio(e) {
    e.preventDefault();
    
    const token = document.getElementById('servicioToken').value;
    const responseDiv = document.getElementById('servicioResponse');
    
    if (!token) {
        responseDiv.innerHTML = '<p style="color: #ef4444;">❌ Por favor, ingresa un token</p>';
        return;
    }
    
    try {
        responseDiv.innerHTML = '<p style="color: #f59e0b;">⏳ Enviando petición...</p>';
        
        const startTime = Date.now();
        const response = await fetch(`${API_BASE_URL}/servicio?token=${encodeURIComponent(token)}`, {
            method: 'GET',
        });
        
        const endTime = Date.now();
        const duration = endTime - startTime;
        const data = await response.json();
        
        if (!response.ok) {
            if (response.status === 401) {
                showAlert('❌ No hay sesión activa. Por favor, inicia sesión primero.', 'error');
            }
        }
        
        const responseType = response.ok ? 'success' : 'error';
        showResponse(responseDiv, response.status, data, duration, responseType);
    } catch (error) {
        showResponse(responseDiv, 0, { error: error.message }, 0, 'error');
    }
}


// ==================== CERRAR SESIÓN ====================
async function handleLogout(e) {
    e.preventDefault();
    
    const token = document.getElementById('logoutToken').value;
    const responseDiv = document.getElementById('logoutResponse');
    
    if (!token) {
        responseDiv.innerHTML = '<p style="color: #ef4444;">❌ Por favor, ingresa un token</p>';
        return;
    }
    
    // Usar modal de confirmación
    const confirmed = await showConfirmModal(
        '🚪 Cerrar Sesión',
        '¿Está seguro de que desea cerrar la sesión?',
        'Cerrar Sesión',
        'warning'
    );
    
    if (!confirmed) {
        return;
    }
    
    try {
        responseDiv.innerHTML = '<p style="color: #f59e0b;">⏳ Enviando petición...</p>';
        
        const startTime = Date.now();
        const response = await fetch(`${API_BASE_URL}/logout`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ token }),
        });
        
        const endTime = Date.now();
        const duration = endTime - startTime;
        const data = await response.json();
        
        if (response.ok) {
            // Limpiar sesión local
            currentToken = null;
            currentEmail = null;
            localStorage.removeItem('sessionToken');
            localStorage.removeItem('sessionEmail');
            
            // Limpiar formularios
            document.getElementById('servicioToken').value = '';
            document.getElementById('logoutToken').value = '';
            
            showResponse(responseDiv, response.status, data, duration, 'success');
        } else {
            showResponse(responseDiv, response.status, data, duration, 'error');
        }
    } catch (error) {
        showResponse(responseDiv, 0, { error: error.message }, 0, 'error');
    }
}

// ==================== ADMIN CLEAR ====================
async function handleAdminClear(e) {
    e.preventDefault();
    
    const confirmed = await showConfirmModal(
        '⚠️ Eliminar Todas las Sesiones',
        '¿Está seguro de que desea eliminar TODAS las sesiones?\n\nEsta acción no se puede deshacer y eliminará todas las sesiones activas del servidor.',
        'Eliminar Todo',
        'danger'
    );
    
    if (!confirmed) {
        return;
    }
    
    const responseDiv = document.getElementById('adminResponse');
    
    try {
        responseDiv.innerHTML = '<p style="color: #f59e0b;">⏳ Enviando petición...</p>';
        
        const startTime = Date.now();
        const response = await fetch(`${API_BASE_URL}/admin/clear`, {
            method: 'POST',
        });
        
        const endTime = Date.now();
        const duration = endTime - startTime;
        const data = await response.json();
        
        if (response.ok) {
            // Limpiar sesión local
            currentToken = null;
            currentEmail = null;
            localStorage.removeItem('sessionToken');
            localStorage.removeItem('sessionEmail');
            
            // Limpiar formularios
            document.getElementById('servicioToken').value = '';
            document.getElementById('logoutToken').value = '';
            
            showResponse(responseDiv, response.status, data, duration, 'success');
        } else {
            showResponse(responseDiv, response.status, data, duration, 'error');
        }
    } catch (error) {
        showResponse(responseDiv, 0, { error: error.message }, 0, 'error');
    }
}

// ==================== FUNCIÓN PARA MOSTRAR RESPUESTAS ====================
function showResponse(container, status, data, duration, type = 'info') {
    // Crear tarjetas modernas con la información (solo tarjetas, sin headers)
    const cardsHTML = createResponseCards(data, type);
    
    container.innerHTML = cardsHTML;
}

// Función para crear tarjetas modernas con la información
function createResponseCards(data, type) {
    const cards = [];
    
    // Si no hay datos o el objeto está vacío
    if (!data || (typeof data === 'object' && Object.keys(data).length === 0)) {
        return '<p class="placeholder">No hay datos en la respuesta</p>';
    }
    
    // Iterar sobre las propiedades del objeto
    for (const [key, value] of Object.entries(data)) {
        const card = createCard(key, value, type);
        cards.push(card);
    }
    
    return cards.length > 0 ? cards.join('') : '<p class="placeholder">No hay datos en la respuesta</p>';
}

// Función para crear una tarjeta individual
function createCard(key, value, type) {
    const icon = getIconForKey(key);
    const formattedKey = formatKey(key);
    const formattedValue = formatValue(value);
    
    return `
        <div class="response-card-simple">
            <div class="card-icon-simple">${icon}</div>
            <div class="card-content-simple">
                <div class="card-label-simple">${formattedKey}</div>
                <div class="card-value-simple">${formattedValue}</div>
            </div>
        </div>
    `;
}

// Función para obtener el ícono según la clave
function getIconForKey(key) {
    const icons = {
        'token': '🔑',
        'mensaje': '💬',
        'correo': '📧',
        'error': '❌',
        'detalle': '📝',
        'password': '🔒',
        'email': '📧'
    };
    return icons[key.toLowerCase()] || '📋';
}

// Función para formatear la clave
function formatKey(key) {
    const formatted = {
        'token': 'Token de Sesión',
        'mensaje': 'Mensaje',
        'correo': 'Correo Electrónico',
        'error': 'Error',
        'detalle': 'Detalle',
        'password': 'Contraseña'
    };
    return formatted[key.toLowerCase()] || key.charAt(0).toUpperCase() + key.slice(1);
}

// Función para formatear el valor
function formatValue(value) {
    if (value === null || value === undefined) {
        return '<span class="null-value">null</span>';
    }
    
    if (typeof value === 'boolean') {
        return `<span class="boolean-value">${value ? '✓ Verdadero' : '✗ Falso'}</span>`;
    }
    
    if (typeof value === 'number') {
        return `<span class="number-value">${value}</span>`;
    }
    
    if (typeof value === 'string') {
        // Si es un token, mostrar más compacto
        if (value.length > 40 && /^\d+_\d+$/.test(value)) {
            return `<span class="token-value" title="${value}">${value.substring(0, 20)}...</span>`;
        }
        return `<span class="string-value">${escapeHtml(value)}</span>`;
    }
    
    if (typeof value === 'object') {
        return `<span class="object-value">${JSON.stringify(value, null, 2)}</span>`;
    }
    
    return escapeHtml(String(value));
}

// Función para escapar HTML
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Función para obtener el tipo de valor
function getValueType(value) {
    if (typeof value === 'string' && value.length > 40) return 'long';
    return typeof value;
}

// Función para obtener la etiqueta del tipo
function getTypeLabel(type) {
    const labels = {
        'success': '✅ Éxito',
        'error': '❌ Error',
        'warning': '⚠️ Advertencia',
        'info': 'ℹ️ Info'
    };
    return labels[type] || 'Info';
}

// ==================== SISTEMA DE MODAL / POPUP ====================
let modalResolve = null;

function showModal(title, message, confirmText = 'Confirmar', onConfirm = null, type = 'warning') {
    document.getElementById('modalTitle').textContent = title;
    document.getElementById('modalMessage').textContent = message;
    
    const confirmBtn = document.getElementById('modalConfirmBtn');
    confirmBtn.textContent = confirmText;
    
    // Cambiar estilo del botón según el tipo
    confirmBtn.className = 'btn';
    if (type === 'danger') {
        confirmBtn.classList.add('btn-danger');
    } else if (type === 'success') {
        confirmBtn.classList.add('btn-primary');
    } else {
        confirmBtn.classList.add('btn-secondary');
    }
    
    // Configurar callback del botón confirmar
    if (onConfirm) {
        confirmBtn.onclick = onConfirm;
    } else {
        confirmBtn.onclick = closeModal;
    }
    
    // Mostrar modal y overlay
    document.getElementById('modalOverlay').classList.add('show');
    document.getElementById('confirmModal').classList.add('show');
}

function closeModal() {
    document.getElementById('modalOverlay').classList.remove('show');
    document.getElementById('confirmModal').classList.remove('show');
    if (modalResolve) {
        modalResolve(false);
        modalResolve = null;
    }
}

function confirmModalAction() {
    if (modalResolve) {
        modalResolve(true);
        modalResolve = null;
    }
    closeModal();
}

// Cerrar modal con ESC
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        closeModal();
    }
});

// Función para mostrar alerta (reemplaza alert())
function showAlert(message, type = 'info') {
    const titles = {
        'info': 'ℹ️ Información',
        'success': '✅ Éxito',
        'error': '❌ Error',
        'warning': '⚠️ Advertencia'
    };
    
    showModal(titles[type] || titles.info, message, 'Aceptar', null, type);
}

// Promesa para confirmación (reemplaza confirm())
function showConfirmModal(title, message, confirmText = 'Confirmar', type = 'warning') {
    return new Promise((resolve) => {
        modalResolve = resolve;
        
        const onConfirm = () => {
            if (modalResolve) {
                modalResolve(true);
                modalResolve = null;
            }
            closeModal();
        };
        
        const onCancel = () => {
            if (modalResolve) {
                modalResolve(false);
                modalResolve = null;
            }
            closeModal();
        };
        
        // Configurar botón de confirmar
        const confirmBtn = document.getElementById('modalConfirmBtn');
        confirmBtn.onclick = onConfirm;
        
        // Configurar botón de cancelar
        const cancelBtn = document.querySelector('.modal-footer .btn-secondary');
        if (cancelBtn) {
            cancelBtn.onclick = onCancel;
        }
        
        // Configurar cierre con overlay o ESC
        const overlay = document.getElementById('modalOverlay');
        const closeBtn = document.querySelector('.modal-close');
        
        overlay.onclick = onCancel;
        if (closeBtn) {
            closeBtn.onclick = onCancel;
        }
        
        showModal(title, message, confirmText, onConfirm, type);
    });
}