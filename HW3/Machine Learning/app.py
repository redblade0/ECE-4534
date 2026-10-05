"""Supplied local HTTP/dashboard transport. Student work lives in backend.py.

Instructor mode can load another submission's backend while serving this fixed
frontend. No board, calibration, recording or ML algorithms are implemented here.
"""
import argparse
import importlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
import traceback

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM4', help='Board UART port, e.g. COM5')
    parser.add_argument('--http-port', type=int, default=8768)
    parser.add_argument('--backend-dir', type=Path, default=Path(__file__).parent)
    parser.add_argument('--frontend', type=Path, default=Path(__file__).with_name('index.html'))
    parser.add_argument('--data-dir', type=Path,
                        help='Defaults to the backend directory/artifacts')
    args = parser.parse_args()
    backend_dir = args.backend_dir.resolve()
    sys.path.insert(0, str(backend_dir))
    module = importlib.import_module('backend')
    backend = module.Backend(port=args.port,
                             data_dir=(args.data_dir or backend_dir / 'artifacts').resolve())
    frontend = args.frontend.resolve().read_bytes()
    class Handler(BaseHTTPRequestHandler):
        def reply(self, status, data, content_type='application/json'):
            if content_type == 'application/json':
                data = json.dumps(data, allow_nan=False).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == '/':
                self.reply(200, frontend, 'text/html; charset=utf-8')
            elif self.path == '/api/state':
                try:
                    self.reply(200, backend.snapshot())
                except Exception:
                    traceback.print_exc()
                    self.reply(500, {'error': 'Backend snapshot failed; inspect terminal'})
            else:
                self.reply(404, {'error': 'Unknown endpoint'})

        def do_POST(self):
            if self.path != '/api/action':
                self.reply(404, {'error': 'Unknown endpoint'})
                return
            # Local dashboard requests only; this transport is provided, not graded.
            allowed = {f'http://127.0.0.1:{args.http_port}',
                       f'http://localhost:{args.http_port}'}
            if self.headers.get('Origin') not in allowed | {None}:
                self.reply(403, {'error': 'Origin not allowed'})
                return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= 4096:
                    raise ValueError('Invalid request size')
                payload = json.loads(self.rfile.read(size))
                if not isinstance(payload, dict) or not isinstance(payload.get('command'), str):
                    raise ValueError('Expected an object with a string command')
                backend.action(payload['command'], payload)
                self.reply(200, {'ok': True})
            except NotImplementedError as error:
                self.reply(501, {'error': str(error)})
            except (ValueError, RuntimeError, KeyError) as error:
                self.reply(400, {'error': str(error)})
            except Exception:
                traceback.print_exc()
                self.reply(500, {'error': 'Backend action failed; inspect terminal'})

        def log_message(self, *unused):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', args.http_port), Handler)
    try:
        backend.start()
        print(f'Dashboard: http://127.0.0.1:{args.http_port}/', flush=True)
        print(f'Backend: {backend_dir}; board port: {args.port}', flush=True)
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        backend.close()

if __name__ == '__main__':
    main()
