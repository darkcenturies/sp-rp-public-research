using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
namespace ValkyrieRepair {
public static class CrashfixSupport {
    public const string VersionText="2.2.2";
    public const string Download="https://sp-rp.com/#valkyrie-crashfix";
    public static bool Candidate(string root,string path) {
        string name=Path.GetFileName(path);
        if(!name.Equals("valkyrie-crashfix.asi",StringComparison.OrdinalIgnoreCase) && !name.Equals("Valkyrie Crashfix.asi",StringComparison.OrdinalIgnoreCase))return false;
        string relative=path.Substring(Filesystem.Normalize(root).Length);
        string[] parts=relative.Split(Path.DirectorySeparatorChar);
        return parts.Length==1 || ((parts[0].Equals("scripts",StringComparison.OrdinalIgnoreCase) || parts[0].Equals("plugins",StringComparison.OrdinalIgnoreCase) || parts[0].Equals("modloader",StringComparison.OrdinalIgnoreCase)) && !parts.Any(p=>p.IndexOf("backup",StringComparison.OrdinalIgnoreCase)>=0 || p.IndexOf("disabled",StringComparison.OrdinalIgnoreCase)>=0));
    }
    public static Version ReadVersion(string file) {
        if(new FileInfo(file).Length>16*1024*1024)return null;
        string data=Encoding.ASCII.GetString(File.ReadAllBytes(file));
        Match match=Regex.Match(data,@"Valkyrie Crashfix (\d+\.\d+(?:\.\d+)?) starting");
        Version version;
        return match.Success && Version.TryParse(match.Groups[1].Value,out version)?version:null;
    }
    public static bool Older(Version version) {
        return version!=null && (version.Major<2 || (version.Major==2 && (version.Minor<2 || (version.Minor==2 && Math.Max(0,version.Build)<2))));
    }
}
}
