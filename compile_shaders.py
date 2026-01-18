import os
import subprocess
import sys

def compile_shaders():
    shader_dir = os.path.join("assets", "shaders")
    if not os.path.exists(shader_dir):
        print(f"Error: Directory {shader_dir} does not exist.")
        return

    extensions = [".vert", ".frag", ".comp", ".geom", ".tesc", ".tese"]
    
    for filename in os.listdir(shader_dir):
        base, ext = os.path.splitext(filename)
        if ext in extensions:
            input_path = os.path.join(shader_dir, filename)
            output_path = input_path + ".spv"
            
            print(f"Compiling {filename}...")
            result = subprocess.run(["glslc", input_path, "-o", output_path], capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"Error compiling {filename}:")
                print(result.stderr)
            else:
                print(f"Successfully compiled {filename}")

if __name__ == "__main__":
    compile_shaders()
