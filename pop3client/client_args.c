#include <client_args.h>

static void help(const int argc) {
    if (argc==2)
    {
        printf(
            "\n-------------------------- HELP --------------------------\n"
            "Usage: ./pop3client [opciones] <servidor-admin>\n"
            "\n"
            "  -h                            Imprime la ayuda y termina.\n"
            "  -t                            Admin auth [REQUIRED]\n "
            "  -P <puerto-admin>             Puerto  donde  se  encuentra  el  servidor  admin.\n"
            "\n"
        );
        exit(0);
    }
    fprintf(stderr, "Invalid use of -h option.\n");
    exit(1);
}

void 
parse_args(const int argc, char **argv, struct client_args *client_args) {
    
    memset(client_args, 0, sizeof(*client_args));

    if (argc < 2) {
        fprintf(stderr, "It is required to specify server address.");
        exit(1);
    }

    client_args->admin_auth           = NULL;
    client_args->admin_server_address = argv[argc - 1];
    client_args->admin_server_port    = 9090;
    
    int c;
    while ((c=getopt(argc, argv, "hP:t:")) != -1) {
        switch (c) {
            case 'h':
                help(argc);
                break;
            case 'P':
                client_args->admin_server_port = atoi(optarg);
                break;
            case 't':
                client_args->admin_auth = optarg;
                break;
            case '?':
                if(optopt == 'P' || optopt == 't') {
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
    // if (optind < argc) {
    //     fprintf(stderr, "Argument not accepted: ");
    //     while (optind < argc) {
    //         fprintf(stderr, "%s ", argv[optind++]);
    //     }
    //     fprintf(stderr, "\n");
    //     exit(1);
    // } 
}
