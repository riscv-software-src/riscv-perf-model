import json
from pathlib import Path
from collections import defaultdict

class VectorTestGenerator:
    def __init__(self, mavis_json_dir):
        self.mavis_json_dir = Path(mavis_json_dir)
        self.vector_instructions = []
    
    def load_mavis_json_files(self):
        json_files = list(self.mavis_json_dir.glob("*.json"))
        
        for json_file in json_files:
            if not json_file.exists():
                continue
            
            try:
                with open(json_file, 'r') as file:
                    instructions = json.load(file)
                    if isinstance(instructions, list):
                        for inst in instructions:
                            mnemonic = inst.get("mnemonic", "")
                            tags = inst.get("tags", [])
                            inst_type = inst.get("type", [])
                            
                            if mnemonic.startswith("v") or \
                               (isinstance(tags, list) and "v" in tags) or \
                               (isinstance(inst_type, list) and "vector" in inst_type):
                                self.vector_instructions.append(inst)
            except json.JSONDecodeError:
                print(f"Error reading {json_file}, skipping")

    def get_operand_fields(self, inst):
        sources = []
        dests = []
        others = []
        
        mnemonic = inst.get("mnemonic", "")
        
        if mnemonic in ["vsetvli", "vsetvl"]:
            dests.append("rd")
            sources.append("rs1")
            others.append("vtype")
            return list(set(sources)), list(set(dests)), list(set(others))
        
        if mnemonic == "vsetivli":
            dests.append("rd")
            sources.append("rs1")
            others.extend(["imm", "vtype"])
            return list(set(sources)), list(set(dests)), list(set(others))
        
        l_oper = inst.get("l-oper", [])
        if not mnemonic.startswith("vsetivli"):
            if isinstance(l_oper, list):
                for op in l_oper:
                    if op == "rd":
                        dests.append("rd")
                    else:
                        sources.append(op)
            elif l_oper == "all":
                sources.extend(["rs1", "rs2"])
        
        v_oper = inst.get("v-oper", [])
        if isinstance(v_oper, list):
            for op in v_oper:
                if op == "rd":
                    dests.append("vd")
                elif op == "rs1":
                    sources.append("vs1")
                elif op == "rs2":
                    sources.append("vs2")
                elif op == "rs3":
                    sources.append("vs3")
        elif v_oper == "all":
            dests.append("vd")
            sources.extend(["vs1", "vs2"])
        
        d_oper = inst.get("d-oper", [])
        if isinstance(d_oper, list):
            for op in d_oper:
                if op == "rs1":
                    sources.append("fs1")
                elif op == "rd":
                    dests.append("rd")
                else:
                    sources.append(op)
        
        s_oper = inst.get("s-oper", [])
        if isinstance(s_oper, list):
            sources.extend(s_oper)
        
        inst_type = inst.get("type", [])
        
        if isinstance(inst_type, list) and "store" in inst_type:
            if "rs3" in sources:
                sources.remove("rs3")
            if "vs3" not in sources:
                sources.append("vs3")
            if "rs1" not in sources:
                sources.append("rs1")

        if isinstance(inst_type, list) and "load" in inst_type:
            if "rd" not in dests and "vd" not in dests:
                dests.append("vd")
            if "rs1" not in sources:
                sources.append("rs1")
        
        if ".vv" in mnemonic:
            if "vs1" not in sources: sources.append("vs1")
            if "vs2" not in sources: sources.append("vs2")
            if "vd" not in dests: dests.append("vd")
        elif ".vx" in mnemonic:
            if "rs1" not in sources: sources.append("rs1")
            if "vs2" not in sources: sources.append("vs2")
            if "vd" not in dests: dests.append("vd")
        elif ".vi" in mnemonic:
            if "vs2" not in sources: sources.append("vs2")
            if "vd" not in dests: dests.append("vd")
            if "imm" not in others: others.append("imm")
        elif ".vf" in mnemonic:
            if "fs1" not in sources: sources.append("fs1")
            if "vs2" not in sources: sources.append("vs2")
            if "vd" not in dests: dests.append("vd")
        
        return list(set(sources)), list(set(dests)), list(set(others))
    
    def generate_operand_values(self, sources, dests, others):
        operands = {}
        reg = 8 
        
        SCALAR_REGS = {"rs1": 3, "rs2": 5}
        
        for dest in dests:
            if dest == "rd": 
                operands["rd"] = 0
            elif dest == "vd": 
                operands[dest] = reg
                reg += 4
        
        for src in sources:
            if src in SCALAR_REGS:
                operands[src] = SCALAR_REGS[src]
            elif src in ["vs1", "vs2", "vs3", "fs1", "fs2"]:
                if src not in operands:
                    operands[src] = reg
                    reg += 4
        
        for other in others:
            if other == "imm": 
                operands["imm"] = 5
            elif other == "vaddr": 
                operands["vaddr"] = "0x1000"
            elif other == "vtype":
                operands["vtype"] = "0x10"
                operands["vl"] = 32
                operands["vta"] = 1
            
        return operands
    
    def generate_tests(self, output_dir):
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        groups = defaultdict(list)
        for inst in self.vector_instructions:
            mnemonic = inst.get("mnemonic", "")
            inst_type = inst.get("type", [])
            
            if isinstance(inst_type, list):
                if "load" in inst_type:
                    group = "load"
                elif "store" in inst_type:
                    group = "store"
                elif "mac" in inst_type:
                    group = "mac"
                elif mnemonic.startswith("vset"):
                    group = "vset"
                elif "reduction" in inst_type or mnemonic.startswith("vred"):
                    group = "reduction"
                elif "add" in mnemonic:
                    group = "add"
                elif "mul" in mnemonic:
                    group = "multiply"
                elif any(x in mnemonic for x in ["seq", "slt", "sne", "sgt", "sge", "sle"]):
                    group = "compare"
                elif "slide" in mnemonic:
                    group = "slide"
                elif "gather" in mnemonic:
                    group = "gather"
                elif "permute" in mnemonic or ("mv" in mnemonic and not mnemonic.startswith("vset")):
                    group = "permute"
                else:
                    group = "other"
            else:
                if mnemonic.startswith("vset"):
                    group = "vset"
                elif mnemonic.startswith("vl"):
                    group = "load"
                elif mnemonic.startswith("vs") and not mnemonic.startswith("vset"):
                    group = "store"
                else:
                    group = "other"
            
            groups[group].append(inst)
        
        for group_name, instructions in groups.items():
            if not instructions: continue
            
            all_tests = []
            for inst in instructions:
                mnemonic = inst.get("mnemonic", "")
                if not mnemonic.startswith("vset"):
                    all_tests.append({
                        "mnemonic": "vsetivli",
                        "rd": 0, "imm": 260, "vtype": "0x12", "vl": 128, "vta": 0
                    })
                
                sources, dests, others = self.get_operand_fields(inst)
                test_inst = {"mnemonic": mnemonic}
                test_inst.update(self.generate_operand_values(sources, dests, others))
                
                inst_type = inst.get("type", [])
                if isinstance(inst_type, list) and ("load" in inst_type or "store" in inst_type):
                    test_inst["vaddr"] = "0x1000"
                
                all_tests.append(test_inst)
            
            output_file = output_path / f"{group_name}.json"
            with open(output_file, 'w') as f:
                json.dump(all_tests, f, indent=2)
            print(f"Generated {output_file} with {len(instructions)} instructions")

def main():

    mavis_dir = "./mavis/json"
    output_dir = "./jsonTests"

    generator = VectorTestGenerator(mavis_dir)
    generator.load_mavis_json_files()
    generator.generate_tests(output_dir)

if __name__ == "__main__":
    main()
