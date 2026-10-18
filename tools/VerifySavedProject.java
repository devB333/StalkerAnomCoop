// Read-only check that a persisted Ghidra Program reopens with analyzed state.
import ghidra.app.script.GhidraScript;
import java.io.PrintWriter;

public class VerifySavedProject extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[0], "UTF-8")) {
            out.println("program=" + currentProgram.getName());
            out.println("sha256=" + currentProgram.getExecutableSHA256());
            out.println("image_base=" + currentProgram.getImageBase());
            out.println("function_count=" + currentProgram.getFunctionManager().getFunctionCount());
            out.println("symbol_count=" + currentProgram.getSymbolTable().getNumSymbols());
            out.println("language=" + currentProgram.getLanguageID());
        }
    }
}
