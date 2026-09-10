using System;
using System.Windows.Forms;

namespace ValkyrieRepair {
static class Program {
    [STAThread]
    static int Main(string[] args) {
        if (args.Length > 0 && args[0].Equals("--self-test", StringComparison.OrdinalIgnoreCase))
            return SelfTests.Run(args.Length > 1 ? args[1] : null);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
        return 0;
    }
}
}
