﻿using System.IO;

namespace MiniSQL.Executor.Interface
{
    public static partial class InterfaceExtensions
    {
        public static void WriteLines(this StreamWriter writer, params object[] str)
        {
            foreach (string s in str)
            {
                writer.WriteLine(s);
            }
        }
    }
}