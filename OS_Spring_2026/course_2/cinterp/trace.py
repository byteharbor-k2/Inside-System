# Trace: 
# Runs a -g compiled binary step by step: Collect all statements (in the
# program, not library) executed.  Regard statements as state transitions, also
# collect all states, with flattened variables as 1D key-value.  Save to a
# textual "trace.log".
#
# Use: gdb -q -x tracer.py --eval-command="trace-run" --eval-command="quit" ./my_program


import gdb
import linecache
import os

class TraceRunCommand(gdb.Command):
    """
    Runs a -g compiled binary step by step.
    Collects states (flattened variables) and statements (transitions).
    Usage: trace-run
    """
    def __init__(self):
        super(TraceRunCommand, self).__init__("trace-run", gdb.COMMAND_USER)
        # Directories to ignore (Standard libraries, glibc, etc.)
        self.ignore_paths = ['/usr/', '/lib/', '/sys/']
        self.filename = None

    def invoke(self, arg, from_tty):
        gdb.execute("set pagination off")
        gdb.execute("set confirm off")
        
        try:
            gdb.execute("start", to_string=True)
        except gdb.error:
            print("Error: Could not start the program. Is a binary loaded?")
            return

        with open("trace.log", "w") as log:
            log.write("=== EXECUTION TRACE START ===\n\n")
            step_count = 0

            while True:
                frame = gdb.selected_frame()
                sal = frame.find_sal()
                
                # Check if we stepped out of bounds or program ended
                if not sal or not sal.symtab:
                    try:
                        gdb.execute("step", to_string=True)
                        continue
                    except gdb.error:
                        break # Program exited

                filename = sal.symtab.fullname()
                if not self.filename:
                    self.filename = filename
                
                # Filter out standard libraries
                if any(filename.startswith(p) for p in self.ignore_paths):
                    try:
                        gdb.execute("finish", to_string=True) # Run until library function returns
                    except gdb.error:
                        try:
                            gdb.execute("step", to_string=True)
                        except gdb.error:
                            break
                    continue

                # Extract Statement (State Transition Trigger)
                line_no = sal.line
                source_line = linecache.getline(filename, line_no).strip()
                
                # Extract and flatten state
                state = self.extract_state(frame)

                if filename == self.filename:

                    # Write to trace.log
                    log.write("State      :\n")
                    if not state:
                        log.write("  (No local variables)\n")
                    else:
                        for k, v in sorted(state.items()):
                            if "__" not in k:
                                log.write(f"  {k} = {v}\n")
                    log.write("\n")

                    log.write(f"Transition : {os.path.basename(filename)}:{line_no} -> {source_line}\n")
                    log.write("\n")

                    step_count += 1

                # Execute the step
                try:
                    gdb.execute("step", to_string=True)
                except gdb.error as e:
                    if "exited" in str(e) or "not being run" in str(e):
                        log.write("=== PROGRAM EXITED ===\n")
                        break
                    else:
                        gdb.execute("next", to_string=True)

        print("Trace complete. Output saved to 'trace.log'.")

    def extract_state(self, frame):
        """Extracts and flattens all local variables in the current frame."""
        variables = {}
        try:
            block = frame.block()
        except RuntimeError:
            return variables

        while block:
            if not block.is_global and not block.is_static:
                for symbol in block:
                    if symbol.is_argument or symbol.is_variable:
                        try:
                            val = frame.read_var(symbol, block)
                            variables.update(self.flatten_value(symbol.name, val))
                        except ValueError:
                            variables[symbol.name] = "<optimized out>"
                        except Exception as e:
                            variables[symbol.name] = f"<error: {e}>"
            block = block.superblock
        return variables

    def flatten_value(self, name, val, depth=0, max_depth=3):
        """Recursively flattens structs, arrays, and pointers into 1D key-values."""
        res = {}
        if depth > max_depth:
            return {name: "<max_depth>"}

        if val.is_optimized_out:
            return {name: "<optimized out>"}

        try:
            vtype = val.type.strip_typedefs()
            type_code = vtype.code
        except Exception:
            return {name: str(val)}

        # Handle Structs / Unions
        if type_code in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
            for field in vtype.fields():
                if not field.is_base_class:
                    try:
                        field_val = val[field]
                        res.update(self.flatten_value(f"{name}.{field.name}", field_val, depth + 1, max_depth))
                    except Exception:
                        pass
        # Handle Arrays
        elif type_code == gdb.TYPE_CODE_ARRAY:
            try:
                low, high = vtype.range()
                # Limit array output to first 5 elements to prevent massive logs
                for i in range(low, min(high + 1, low + 5)):
                    res.update(self.flatten_value(f"{name}[{i}]", val[i], depth + 1, max_depth))
                if high - low >= 5:
                    res[f"{name}[...]"] = "<truncated>"
            except Exception:
                res[name] = str(val)
        # Handle Pointers
        elif type_code == gdb.TYPE_CODE_PTR:
            try:
                addr = int(val.cast(gdb.lookup_type('long')))
                res[name] = hex(addr) if addr != 0 else "NULL"
            except Exception:
                res[name] = str(val)
        # Handle basic types
        else:
            res[name] = str(val)

        return res

# Register the command in GDB
TraceRunCommand()
