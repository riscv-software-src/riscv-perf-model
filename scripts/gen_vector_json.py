#!/usr/bin/env python3

# Adding this here for now (idk if theres another place better suited)
#  How to use:
#  You can run the script from the project root:
#  python3 scripts/gen_vector_json.py --count 100 --output traces/vector_auto_gen.json

#  --count: Number of instructions to generate
#  --output: output file path
#  --seed: random seed for reproducibility

import argparse
import json
import os
import random
import sys
import glob

def parse_args():
    parser = argparse.ArgumentParser(description="Generate synthetic Vector JSON traces for Olympia.")
    parser.add_argument("-o", "--output", default="traces/vector_auto_gen.json", help="Output JSON file path.")
    parser.add_argument("-n", "--count", type=int, default=100, help="Number of instructions to generate.")
    parser.add_argument("--seed", type=int, default=42, help="Random seed.")
    return parser.parse_args()

def main():
    args = parse_args()
    random.seed(args.seed)

    mavis_path = "mavis"
    print(f"Generating {args.count} vector instructions to {args.output}")
    print(f"Using Mavis definitions from: {mavis_path}")

    instructions = load_mavis_instructions(mavis_path)
    print(f"Found {len(instructions)} vector instructions.")

    if not instructions:
        print("No vector instructions found. Exiting.")
        sys.exit(1)

    generate_trace(instructions, args.count, args.output)

def load_mavis_instructions(mavis_path):
    json_dir = os.path.join(mavis_path, "json")
    if not os.path.isdir(json_dir):
        print(f"Error: {json_dir} does not exist.")
        sys.exit(1)

    # pattern for vector extensions
    patterns = ["isa_rv64zve*.json", "isa_rv64v*.json", "isa_rv32zve*.json"] 
    files = []
    for p in patterns:
        files.extend(glob.glob(os.path.join(json_dir, p)))
    
    vector_insts = []
    
    for fpath in files:
        with open(fpath, 'r') as f:
            try:
                data = json.load(f)
                if isinstance(data, list):
                    for inst in data:
                        # Check if it's a vector instruction
                        # Filter by tags "v" or type "vector"
                        is_vector = False
                        if "tags" in inst and "v" in inst["tags"]:
                            is_vector = True
                        elif "type" in inst and "vector" in inst["type"]:
                            is_vector = True

                        if is_vector:
                            vector_insts.append(inst)
            except json.JSONDecodeError:
                print(f"Warning: Failed to decode {fpath}")

    return vector_insts

def generate_trace(instructions, count, output_file):
    trace = []
    
    for _ in range(count):
        inst_def = random.choice(instructions)
        mnemonic = inst_def["mnemonic"]
        
        entry = { "mnemonic": mnemonic }
        
        # Determine Operands
        # V-Oper (Vector Operands)
        v_oper = inst_def.get("v-oper", [])
        if v_oper == "all":
            v_oper = ["rd", "rs1", "rs2"] 
        
        for op in v_oper:
            if op == "rd": entry["vd"] = random.randint(0, 31)
            elif op == "rs1": entry["vs1"] = random.randint(0, 31)
            elif op == "rs2": entry["vs2"] = random.randint(0, 31)
            elif op == "rs3": entry["vs3"] = random.randint(0, 31)
        
        # D-Oper (Discrete/Scalar Operands) and L-Oper (Load/Store addr)
        d_oper = inst_def.get("d-oper", [])
        l_oper = inst_def.get("l-oper", [])
        
        scalar_ops = []
        if isinstance(d_oper, list): scalar_ops.extend(d_oper)
        if isinstance(l_oper, list): scalar_ops.extend(l_oper)
        
        is_float = False
        if "type" in inst_def and "float" in inst_def["type"]:
            is_float = True
            
        for op in scalar_ops:
            if op == "rs1":
                if mnemonic.endswith(".vf") or mnemonic.endswith(".wf"): 
                    # Float scalar (widening .wf also takes float scalar)
                    entry["fs1"] = random.randint(0, 31)
                elif mnemonic.endswith(".vx") or mnemonic.endswith(".wx") or mnemonic.endswith(".vi"):
                    if mnemonic.endswith(".vi"):
                         entry["imm"] = random.randint(0, 15)
                    else:
                         entry["rs1"] = random.randint(0, 31)
                elif "load" in inst_def.get("type", []) or "store" in inst_def.get("type", []):
                    entry["rs1"] = random.randint(0, 31) # Base address
                    entry["vaddr"] = "0x1000" # Dummy vaddr
                else:
                     entry["rs1"] = random.randint(0, 31)
            elif op == "rs2":
                 # Stride or store data (if scalar store?)
                 entry["rs2"] = random.randint(0, 31)
            elif op == "rd":
                # Scalar destination 
                 if is_float and not (mnemonic.endswith(".x.f.v") or mnemonic.endswith(".x.f.w")): 
                     # Float dest unless conversion to integer
                     entry["fd"] = random.randint(0, 31)
                 else:
                     entry["rd"] = random.randint(0, 31)

        # Handle explicit .vi if d-oper didn't trigger it
        if mnemonic.endswith(".vi") and "imm" not in entry:
            entry["imm"] = random.randint(0, 15)

        # Vector Config
        entry["vl"] = 32
        entry["vtype"] = "0x18" # SEW=64, LMUL=1
        
        trace.append(entry)
        
    with open(output_file, "w") as f:
        json.dump(trace, f, indent=4)
        
    print(f"Generated Trace at {output_file}")

if __name__ == "__main__":
    main()
