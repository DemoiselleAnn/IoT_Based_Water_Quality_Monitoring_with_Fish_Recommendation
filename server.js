const http = require('http');
const fs = require('fs');
const path = require('path');

const host = process.env.HOST || '0.0.0.0';
const port = Number(process.env.PORT || 3000);
const root = __dirname;
const maxHistory = 1000;
const readings = [];

const contentTypes = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.png': 'image/png',
  '.svg': 'image/svg+xml'
};

const send = (res, status, body, type = 'application/json; charset=utf-8') => {
  res.writeHead(status, {
    'Content-Type': type,
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET,POST,OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type'
  });
  res.end(body);
};

const readBody = (req) => new Promise((resolve, reject) => {
  let body = '';
  req.on('data', (chunk) => {
    body += chunk;
    if (body.length > 1024 * 32) {
      reject(new Error('Request body too large'));
      req.destroy();
    }
  });
  req.on('end', () => resolve(body));
  req.on('error', reject);
});

const numberOrNull = (value) => {
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
};

const latestReading = () => readings.at(-1) || null;

const saveReading = (payload) => {
  const temperatureC = numberOrNull(payload.temperatureC);
  const turbidityNTU = numberOrNull(payload.turbidityNTU);
  if (temperatureC == null && turbidityNTU == null) {
    const error = new Error('temperatureC or turbidityNTU is required');
    error.status = 400;
    throw error;
  }

  const receivedAt = new Date().toISOString();
  const reading = {
    deviceId: String(payload.deviceId || 'aquasense-esp32'),
    temperatureC,
    turbidityNTU,
    turbidityRaw: numberOrNull(payload.turbidityRaw),
    turbidityVoltage: numberOrNull(payload.turbidityVoltage),
    temperatureStatus: payload.temperatureStatus || null,
    turbidityCondition: payload.turbidityCondition || null,
    receivedAt,
    connected: true
  };
  readings.push(reading);
  if (readings.length > maxHistory) readings.splice(0, readings.length - maxHistory);
  return reading;
};

const historyRecords = () => readings
  .filter((reading) => reading.temperatureC != null && reading.turbidityNTU != null)
  .map((reading) => ({
    timestamp: reading.receivedAt,
    temperature: reading.temperatureC,
    turbidity: reading.turbidityNTU,
    turbidity_condition: reading.turbidityCondition || undefined
  }));

const serveStatic = (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const routePath = decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname);
  const filePath = path.resolve(root, `.${routePath}`);
  if (!filePath.startsWith(root)) {
    send(res, 403, 'Forbidden', 'text/plain; charset=utf-8');
    return;
  }
  fs.readFile(filePath, (error, data) => {
    if (error) {
      send(res, 404, 'Not found', 'text/plain; charset=utf-8');
      return;
    }
    send(res, 200, data, contentTypes[path.extname(filePath).toLowerCase()] || 'application/octet-stream');
  });
};

const server = http.createServer(async (req, res) => {
  if (req.method === 'OPTIONS') {
    send(res, 204, '');
    return;
  }

  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);

  try {
    if (req.method === 'GET' && url.pathname === '/config.js') {
      const config = {
        apiBaseUrl: '',
        temperatureRangeC: { min: 24, max: 29, criticalMin: 18, criticalMax: 34 },
        turbidityRangeNTU: { clearMax: 25, moderateMax: 50 },
        turbidityUnit: 'NTU',
        fishSpecies: [
          { name: 'Guppy', preferredTemperature: { min: 22, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'A small, colorful freshwater aquarium fish.' },
          { name: 'Molly', preferredTemperature: { min: 24, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'A peaceful livebearing freshwater aquarium fish.' },
          { name: 'Betta', preferredTemperature: { min: 24, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'A freshwater fish known for its flowing fins.' },
          { name: 'Platy', preferredTemperature: { min: 21, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'A small, active livebearing freshwater fish.' },
          { name: 'Neon Tetra', preferredTemperature: { min: 21, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'A small schooling freshwater aquarium fish.' },
          { name: 'Swordtail', preferredTemperature: { min: 22, max: 28 }, preferredTurbidity: { conditions: ['Clear'] }, description: 'An active livebearing freshwater aquarium fish.' }
        ]
      };
      send(res, 200, `window.AQUASENSE_DEMO_MODE = false;\nwindow.AQUASENSE_CONFIG = ${JSON.stringify(config, null, 2)};\n`, 'text/javascript; charset=utf-8');
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/reading') {
      send(res, 200, JSON.stringify(latestReading() || {}));
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/history') {
      send(res, 200, JSON.stringify(historyRecords()));
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/readings') {
      const payload = JSON.parse(await readBody(req) || '{}');
      send(res, 201, JSON.stringify({ ok: true, reading: saveReading(payload) }));
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/reset') {
      readings.length = 0;
      send(res, 200, JSON.stringify({ ok: true }));
      return;
    }

    if (req.method === 'GET') {
      serveStatic(req, res);
      return;
    }

    send(res, 405, JSON.stringify({ error: 'Method not allowed' }));
  } catch (error) {
    send(res, error.status || 500, JSON.stringify({ error: error.message || 'Server error' }));
  }
});

server.listen(port, host, () => {
  console.log(`AquaSense dashboard running at http://localhost:${port}`);
  console.log(`ESP32 endpoint: http://<your-computer-ip>:${port}/api/readings`);
});
