#include <args.h>

static unsigned short port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}

/* static void user(char *s, struct users *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }

} */

static void version(const int argc) {
    if (argc == 2){
        printf( 
            "Pop3filter version 0.0.1\n"
            "ITBA Protocolos de Comunicación 2021 -- Grupo 3\n"
        );
        exit(0);
    }
    fprintf(stderr, "Invalid use of -v option.\n");
    exit(1);
}

static void help(const int argc) {
    if (argc==2)
    {
        printf(
            "\n-------------------------- HELP --------------------------\n"
            "Usage: ./pop3filter [opciones] <servidor-origen>\n"
            "\n"
            "   -e <archivo-de-error>           Path del archivo donde se rediecciona stderr de las ejecuciones de los filtros.\n"
            "   -h                              Imprime la ayuda y termina.\n"
            "   -l <dirección-pop3>             Dirección  donde servirá el proxy POP3\n"
            "   -L <dirección-de-management>    Dirección donde servirá el  servicio  de  management.\n"
            "   -o <puerto-de-managment>        Puerto  donde  se  encuentra  el  servidor  de  management.\n"
            "   -p <puerto-local>               Puerto TCP donde escuchará por conexiones entrantes POP3.\n"
            "   -P <puerto-origen>              Puerto  TCP  donde  se encuentra el servidor POP3 en el servidor origen.\n"
            "   -t <cmd>                        Comando utilizado para las transformaciones externas.\n"
            "   -v                              Imprime información sobre la versión versión y termina.\n"
            "\n"
        );
        exit(0);
    }
    fprintf(stderr, "Invalid use of -h option.\n");
    exit(1);
}

void parse_args(const int argc, char **argv, struct pop3args *args) {
    
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    if (argc < 2) {
        fprintf(stderr, "POP3 requires you to specify origin address.");
        exit(1);
    }

    args->origin_address = argv[argc - 1];
    args->pop3_listen_address         = "0.0.0.0";
    args->pop3_port                   = 1110;
    args->management_listen_address   = "127.0.0.1";
    args->management_port             = 9090;
    args->filter                      = NULL;
    args->error_file                  = "/dev/null";
    args->origin_port                 = 110;

    int c;

    while ((c=getopt(argc, argv, "e:hl:L:o:p:P:t:v")) != -1) {
        switch (c) {
            case 'e':
                args->error_file = optarg;
                break;
            case 'h':
                help(argc);
                break;
            case 'l':
                args->pop3_listen_address = optarg;
                break;
            case 'L':
                args->management_listen_address = optarg;
                break;
            case 'o':
                args->management_port = port(optarg);
                break;
            case 'p':
                args->pop3_port = port(optarg);
                break;
            case 'P':
                args->origin_port = port(optarg);
                break;
            case 't':
                args->filter = optarg;
                break;
            case 'v':
                version(argc);
                break;
            case '?':
                if(optopt == 'e' || optopt == 'l' || optopt == 'L' || optopt == 'o' || optopt == 'p' || optopt == 'P' || optopt == 't') {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                } else if(isprint(optopt)) {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                } else {
                    fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt);
                }
                break;
            default:
                fprintf(stderr, "Invalid options, print help using -h.\n");
                exit(1);
        }

    }
    /* if (optind < argc) {
        fprintf(stderr, "Argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    } */
}
