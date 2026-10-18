// Read-only analysis of selected string references in an existing persistent project.
// No executable patches, symbol renames or database deletion.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.PrintWriter;
import java.util.HashSet;

public class FocusedEvidence extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[0], "UTF-8")) {
            out.println("Program: " + currentProgram.getName());
            out.println("SHA256: " + currentProgram.getExecutableSHA256());
            out.println("Image base: " + currentProgram.getImageBase());
            out.println("Functions: " + currentProgram.getFunctionManager().getFunctionCount());
            DecompInterface dec = new DecompInterface();
            dec.openProgram(currentProgram);
            HashSet<Address> seen = new HashSet<>();
            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(args[i]);
                out.println("Seed string address: " + address);
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(address);
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Function f = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
                    out.println("Reference: " + ref + "; function=" + (f == null ? "unknown" : f.getName() + "@" + f.getEntryPoint()));
                    if (f == null || seen.contains(f.getEntryPoint()) || seen.size() >= 4) continue;
                    seen.add(f.getEntryPoint());
                    out.println("Signature (inferred): " + f.getSignature());
                    DecompileResults result = dec.decompileFunction(f, 60, monitor);
                    out.println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : "DECOMPILE INCOMPLETE: " + result.getErrorMessage());
                }
            }
            dec.dispose();
        }
    }
}
