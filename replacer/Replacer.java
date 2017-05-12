
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.PrintStream;
import java.io.IOException;


class Replacer {

public static void main(String[] args)
{
	if(args.length!=2) {
		PrintStream ps=new PrintStream(System.err);
		ps.println("usage:");
		ps.println("java Replacer replaced replaced-with");
		return;
	}
	BufferedReader in=new BufferedReader(new InputStreamReader(System.in));
	PrintStream out=new PrintStream(System.out);
	try {
		String line;
		while((line=in.readLine())!= null) {
			String replaced=line.replaceAll(args[0],args[1]);
			out.println(replaced);
		}
	} catch(IOException e) {
		new PrintStream(System.err).println(e.getMessage());
	}
}

}

