"""
Simple proxy server that serves the dashboard and proxies API requests to the backend.
This avoids CORS issues by serving everything from the same origin.
"""
from http.server import HTTPServer, SimpleHTTPRequestHandler
import urllib.request
import urllib.error
import subprocess
import json
import tempfile
import os
import sqlite3

class ProxyHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith('/api/') or self.path == '/health':
            self.proxy_request('GET')
        else:
            super().do_GET()
    
    def do_POST(self):
        if self.path.startswith('/api/delete-device'):
            self.handle_delete_device()
        elif self.path.startswith('/api/'):
            self.proxy_request('POST')
        else:
            self.send_error(404)
    
    def do_DELETE(self):
        if self.path.startswith('/api/'):
            self.proxy_request('DELETE')
        else:
            self.send_error(404)
    
    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
    
    def handle_delete_device(self):
        """Handle device deletion by copying db, modifying, copying back"""
        # Parse device_id from query string
        if '?' not in self.path:
            self.send_json(400, {'error': 'Missing id parameter'})
            return
        
        query = self.path.split('?')[1]
        params = {}
        for p in query.split('&'):
            if '=' in p:
                k, v = p.split('=', 1)
                params[k] = urllib.request.unquote(v)
        device_id = params.get('id', '')
        
        if not device_id:
            self.send_json(400, {'error': 'Missing id parameter'})
            return
        
        try:
            # Create temp file for database
            with tempfile.NamedTemporaryFile(delete=False, suffix='.db') as tmp:
                tmp_path = tmp.name
            
            # Copy database from container
            subprocess.run(['docker', 'cp', 'gridpulse-gridpulse-1:/app/build/gridpulse.db', tmp_path], 
                         check=True, capture_output=True)
            
            # Delete device from database
            conn = sqlite3.connect(tmp_path)
            cursor = conn.cursor()
            cursor.execute("DELETE FROM devices WHERE device_id = ?", (device_id,))
            changes = cursor.rowcount
            conn.commit()
            conn.close()
            
            if changes > 0:
                # Copy database back to container
                subprocess.run(['docker', 'cp', tmp_path, 'gridpulse-gridpulse-1:/app/build/gridpulse.db'],
                             check=True, capture_output=True)
                # Restart backend to refresh cache
                subprocess.run(['docker', 'restart', 'gridpulse-gridpulse-1'], capture_output=True)
                self.send_json(200, {'success': True, 'message': 'Device deleted'})
            else:
                self.send_json(404, {'error': 'Device not found'})
            
            # Cleanup
            os.unlink(tmp_path)
            
        except subprocess.CalledProcessError as e:
            self.send_json(500, {'error': f'Docker error: {e.stderr.decode() if e.stderr else str(e)}'})
        except Exception as e:
            self.send_json(500, {'error': str(e)})
    
    def send_json(self, status, data):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())
    
    def proxy_request(self, method):
        backend_url = f'http://localhost:8080{self.path}'
        
        try:
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length) if content_length > 0 else None
            
            req = urllib.request.Request(backend_url, data=body, method=method)
            req.add_header('Content-Type', 'application/json')
            
            with urllib.request.urlopen(req, timeout=10) as response:
                data = response.read()
                
                self.send_response(response.status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(data)
                
        except urllib.error.HTTPError as e:
            self.send_response(e.code)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(e.read())
        except Exception as e:
            self.send_error(502, f'Backend error: {str(e)}')

if __name__ == '__main__':
    port = 3000
    print(f'Starting GridPulse Dashboard at http://localhost:{port}')
    print('Press Ctrl+C to stop')
    server = HTTPServer(('localhost', port), ProxyHandler)
    server.serve_forever()
