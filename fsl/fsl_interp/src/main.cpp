#include "Options.h"
#include "Msg.h"
#include "FslParser.h"
#include <iostream>

Options *Options::instance = 0;
std::shared_ptr<Options> opts(Options::getInstance());

Msg *Msg::instance = 0;
std::unique_ptr<Msg> msg(Msg::getInstance());

FslParser *FP;

int main(int ac,char **av) {
  opts->setupOptions(ac,av);
  FslParser fsl;

  if(opts->trace_en) fsl.TRACE_EN = true;

  fsl.parse();
//    std::cout << "Starting parser...\n";
//    if (yyparse() == 0) {  // Call the parse function
//        std::cout << "Parsing complete!\n";
//    } else {
//        std::cerr << "Parsing failed.\n";
//    }
    return 0;
}

