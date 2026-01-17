import subprocess
import sys

def main():
    cmd = ["cmake", "--build", "build", "--config", "Release"]
    print(f"Running: {' '.join(cmd)}")
    
    try:
        # Run command and capture output
        # explicit encoding None to get bytes
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        
        output_bytes = result.stdout
        
        # Try decoding
        encodings = ['utf-8', 'utf-16', 'mbcs', 'cp1252', 'latin1']
        decoded = None
        used_enc = ""
        
        for enc in encodings:
            try:
                decoded = output_bytes.decode(enc)
                used_enc = enc
                break
            except UnicodeDecodeError:
                continue
                
        if decoded is None:
            decoded = output_bytes.decode('utf-8', errors='ignore')
            used_enc = "utf-8(ignore)"
            
        print(f"Decoded using: {used_enc}")
        
        # Filter and print errors
        lines = decoded.splitlines()
        found_error = False
        for i, line in enumerate(lines):
            if "error" in line.lower() or "warning" in line.lower():
                print(f"[{i}] {line.strip()}")
                found_error = True
                # Print context (next 2 lines)
                if i + 1 < len(lines): print(f"    {lines[i+1].strip()}")
                if i + 2 < len(lines): print(f"    {lines[i+2].strip()}")
        
        if not found_error:
            print("No errors found in output (or pattern mismatch). Dumping last 20 lines:")
            for line in lines[-20:]:
                print(line.strip())
                
    except Exception as e:
        print(f"Script failed: {e}")

if __name__ == "__main__":
    main()
