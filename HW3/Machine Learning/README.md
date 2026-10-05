# Python starter and supplied dashboard

Read ../ASSIGNMENT.md and ../INTERFACE.md. Implement backend.py, acquisition.py
and recognition.py. Comments mark the main responsibilities; they are not a
completed algorithm. You can add modules and change internal function signatures.
Preserve Backend.__init__(port, data_dir), start(), close(), snapshot(), and
action(command, payload). The instructor will load these hooks with a trusted
copy of the dashboard and server. Keep index.html, contract.py and app.py intact
for normal use; changing the UI does not replace a required backend behavior.

Use Python 3.11 or 3.12 and a virtual environment. From this directory:

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\run-dashboard.ps1 --port COM5 --http-port 8768
```

Choose your actual board UART COM port (the debugger may expose another port).
Open the printed URL in a browser. Only one application can own the board's UART
port. If another dashboard is running, use a different HTTP port for the empty
starter; before using real acquisition, close the application owning the COM port.
The empty starter opens the dashboard **without opening serial**. Its controls
remain gated until your backend supplies real connection/calibration state.
Unimplemented actions return clear TODO errors; no simulated readings are supplied.
Stop with Ctrl+C. No global packages or instructor recordings are bundled.

Suggested implementation order:

1. Verify real ADC/UART data, implement parsing and continuous acquisition.
2. Update raw/connected/counters/rate; implement physical calibration and path.
3. Implement countdowns, recording progress, session prompts and saved trials.
4. Train and persist both motion models and the position model using training only.
5. Update live predictions from incoming history, independently of browser polling.
6. Capture the separate evaluation round, evaluate frozen models and save evidence.

Store generated files under artifacts/ (default data_dir); the folder layout and
CSV/evaluation schemas are specified in ../INTERFACE.md. Include your actual
dependency versions and a reproducible launch command in the final submission.
Use ../REPORT_TEMPLATE.md for the written component.
