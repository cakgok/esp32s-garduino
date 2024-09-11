// mockSetup.js
self.addEventListener('install', (event) => {
    self.skipWaiting();
  });
  
  self.addEventListener('activate', (event) => {
    event.waitUntil(clients.claim());
  });
  
  const mockData = {    
    setup: {
      systemSize: 3,
      sdaPin: 21,
      sclPin: 22,
      floatSwitchPin: 23,
      sensorPins: [32, 33, 34],
      relayPins: [25, 26, 27]
    }
  };
  
  self.addEventListener('fetch', (event) => {
    if (event.request.url.endsWith('/api/setup')) {
      if (event.request.method === 'GET') {
        event.respondWith(
          new Response(JSON.stringify(mockData.setup), {
            status: 200,
            headers: { 'Content-Type': 'application/json' }
          })
        );
      } else if (event.request.method === 'POST') {
        event.respondWith(
          event.request.json().then(body => {
            console.log('Received setup update:', body);
            Object.assign(mockData.setup, body);
            return new Response(JSON.stringify({ status: 'success' }), {
              status: 200,
              headers: { 'Content-Type': 'application/json' }
            });
          })
        );
      }
    } else if (event.request.url.endsWith('/api/resetToDefault')) {
      event.respondWith(
        new Response(JSON.stringify({ status: 'success' }), {
          status: 200,
          headers: { 'Content-Type': 'application/json' }
        })
      );
    }
  });