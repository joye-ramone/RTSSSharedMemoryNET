using System;
using System.Linq;
using System.Threading;
using RTSSSharedMemoryNET;

namespace RTSSDemo
{
    class Program
    {
        static void Main(string[] args)
        {
            bool stop = false;

            //enforces a nice cleanup
            //just hitting X or Ctrl+C normally won't actually dispose the using() below
            ExitHandler.Init(ctrlType =>
            {
                Console.WriteLine("\nCleaning up and exiting...");

                stop = true;

                return true; //cancel event
            });

            /////////////////////////////////////////////////////////////////////

            //Console.WriteLine("Current OSD entries:");
            //var osdEntries = OSD.GetOSDEntries();

            //foreach (var osd in osdEntries)
            //{
            //    Console.ForegroundColor = ConsoleColor.Cyan;
            //    Console.WriteLine(osd.Owner);
            //    Console.ResetColor();
            //    Console.WriteLine("{0}\n", osd.Text);
            //}

            /////////////////////////////////////////////////////////////////////

            //Console.WriteLine("Current app entries with GPU contexts:");
            //var appEntries = OSD.GetAppEntries().Where(x => (x.Flags & AppFlags.MASK) != AppFlags.None).ToArray();

            //foreach (var app in appEntries)
            //{
            //    Console.ForegroundColor = ConsoleColor.Magenta;
            //    Console.WriteLine("{0}:{1}", app.ProcessId, app.Name);
            //    Console.ResetColor();
            //    Console.WriteLine("{0}, {1}FPS", app.Flags, app.InstantaneousFrames);
            //}

            //Console.WriteLine("Press any key...");

            /////////////////////////////////////////////////////////////////////

            string predefinedTags = "<P=0,0><A0=-5><A1=4><C0=FFA0A0><C1=FF00A0><C2=FFFFFF><S0=-50><S1=50>";

            string osdText = @"<C0>CPU<S0>1<S><C>	<A0>6<A><A1><S1> %<S><A>";

            float[] history1 = new float[512];
            float[] history2 = new float[512];
            float[] history3 = new float[512];

            for (int i = 0; i < history1.Length; i++)
            {
                history1[i] = 0;
                history2[i] = 0;
                history3[i] = 0;
            }

            uint pos = 0;

            OSD.EMBEDDED_OBJECT_GRAPH dwFlags = OSD.EMBEDDED_OBJECT_GRAPH.FLAG_FILLED;

            var r = new Random();

            //int iteration = 0;

            Console.WriteLine("Start ?");
            Console.ReadLine();

            using (var osd = new OSD("RTSSDemo"))
            {
                while (true)
                {
                    uint dwObjectOffset = 0;
                    uint dwObjectSize = 0;

                    string text = predefinedTags + osdText;

                    text += "\n";

                    float value1 = history1[pos] = (float)(r.Next(0, 150) + r.NextDouble());
                    float value2 = history2[pos] = (float)(r.Next(0, 150) + r.NextDouble());
                    float value3 = history3[pos] = (float)(r.Next(0, 150) + r.NextDouble());

                    pos = (pos + 1) & (512 - 1);

                    dwObjectSize = osd.EmbedGraph(dwObjectOffset, history1, pos, -32, -2, 1, 0.0f, 200, dwFlags);

                    if (dwObjectSize > 0)
                    {
                        //print embedded object
                        string strObj = string.Format("<C0><OBJ={0:X8}><A0><S1>{1:.0}<A> %%<S><C>\n", dwObjectOffset, value1);

                        text += strObj;

                        //modify object offset
                        dwObjectOffset += dwObjectSize;
                    }

                    dwObjectSize = osd.EmbedGraph(dwObjectOffset, history2, pos, -32, -2, 1, 0.0f, 200, dwFlags);

                    if (dwObjectSize > 0)
                    {
                        //print embedded object
                        string strObj = string.Format("<C0><OBJ={0:X8}><A0><S1>{1:.0}<A> %%<S><C>\n", dwObjectOffset, value2);

                        text += strObj;

                        //modify object offset
                        dwObjectOffset += dwObjectSize;
                    }

                    dwObjectSize = osd.EmbedGraph(dwObjectOffset, history3, pos, -32, -2, 1, 0.0f, 200, dwFlags);

                    if (dwObjectSize > 0)
                    {
                        //print embedded object
                        string strObj = string.Format("<C0><OBJ={0:X8}><A0><S1>{1:.0}<A> %%<S><C>\n", dwObjectOffset, value3);

                        text += strObj;

                        //modify object offset
                        dwObjectOffset += dwObjectSize;
                    }

                    Console.WriteLine("Update Position:" + pos);

                    osd.Update(text);

                    if (stop)
                    {
                        return;
                    }

                    Thread.Sleep(TimeSpan.FromMilliseconds(500));

                    //if (iteration < 10)
                    //{
                    //    iteration++;
                    //    Thread.Sleep(TimeSpan.FromSeconds(1));
                    //}
                    //else
                    //{
                    //    var t = Console.ReadLine();
                    //    if (t == null)
                    //        break;

                    //    iteration = 0;
                    //}

                }
            }

            Console.WriteLine("End");
            Console.ReadLine();
        }
    }
}
