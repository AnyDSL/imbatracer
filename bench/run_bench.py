from subprocess import Popen, PIPE
import sys, os
import re
import datetime

# contains a dictionary of settings for every benchmark test
bench_settings = [
    {
        'name': 'Cornell box',
        'scene': 'scenes/cornell/cornell_org.scene',
        'reference': 'references/ref_cornell_org.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'cornell',
        'args': ['-r', '0.003']
    },

    {
        'name': 'Cornell specular balls',
        'scene': 'scenes/cornell/cornell_specular_front.scene',
        'reference': 'references/ref_cornell_specular_front.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'cornell_specular_front',
        'args': ['-r', '0.003']
    },

    {
        'name': 'Cornell specular balls close',
        'scene': 'scenes/cornell/cornell_specular.scene',
        'reference': 'references/ref_cornell_specular.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'cornell_specular',
        'args': ['-r', '0.003']
    },

    {
        'name': 'Cornell indirect',
        'scene': 'scenes/cornell/cornell_indirect.scene',
        'reference': 'references/ref_cornell_indirect.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'cornell_indirect',
        'args': ['-r', '0.003']
    },

    {
        'name': 'Cornell water',
        'scene': 'scenes/cornell/cornell_water.scene',
        'reference': 'references/ref_cornell_water.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'cornell_water',
        'args': ['-r', '0.003']
    },

    {
        'name': 'Sponza behind curtain',
        'scene': 'scenes/sponza/sponza.scene',
        'reference': 'references/ref_sponza_curtain.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'sponza_curtain',
        'args': ['-r', '0.006']
    },

    {
        'name': 'Still Life',
        'scene': 'scenes/stilllife/still_life.scene',
        'reference': 'references/ref_still_life.png',
        'width': 960,
        'height': 540,
        'base_filename': 'still_life',
        'args': ['-r', '0.02', '--max-path-len', '12']
    },
]

scheduler_args = [
    ##############################################################
    # Default thread count, varying spp
    # {
    #     'name': 'default: 256x256 4 threads 1 spp',
    #     'abbr': 'default',
    #     'args': []
    # },

    # {
    #     'name': 'default 2 spp: 256x256 4 threads 2 spp',
    #     'abbr': 'default_spp2',
    #     'args': ['--spp', '2']
    # },

    {
        'name': 'default 4 spp: 256x256 4 threads 4 spp',
        'abbr': 'default_spp4',
        'args': ['--spp', '4']
    },

    ##############################################################
    # Single threaded version for reference
    # {
    #     'name': 'single thread: 1024x1024 1 threads 1 spp',
    #     'abbr': 'single',
    #     'args': ['--thread-count', '1', '--tile-size', '1024']
    # },

    ##############################################################
    # fewer threads, varying spp
    # {
    #     'name': 'fewer threads: 256x256 2 threads 1 spp',
    #     'abbr': 'fewer',
    #     'args': ['--thread-count', '2']
    # },

    # {
    #     'name': 'fewer threads 2 spp: 256x256 2 threads 2 spp',
    #     'abbr': 'fewer_spp2',
    #     'args': ['--spp', '2', '--thread-count', '2']
    # },

    # {
    #     'name': 'fewer threads 4 spp: 256x256 2 threads 4 spp',
    #     'abbr': 'fewer_spp4',
    #     'args': ['--spp', '4', '--thread-count', '2']
    # },

    ##############################################################
    # smaller tiles
#     {
#         'name': 'smaller tiles: 128x128 4 threads 1 spp',
#         'abbr': 'small',
#         'args': ['--tile-size', '128']
#     },

#     {
#         'name': 'smaller tiles 2 spp: 128x128 4 threads 2 spp',
#         'abbr': 'small_spp2',
#         'args': ['--spp', '2', '--tile-size', '128']
#     },

#     {
#         'name': 'smaller tiles 4 spp: 128x128 4 threads 4 spp',
#         'abbr': 'small_spp4',
#         'args': ['--spp', '4', '--tile-size', '128']
#     }
]

alg_small = ['pt', 'bpt', 'vcm']
alg_large = ['pt', 'bpt', 'vcm', 'lt', 'ppm']
alg_pt_only = ['pt']

times_in_seconds = [5] #[5, 10, 30, 60]
algorithms = ['pt']
convergence = True
convergence_step_sec = 1

def compute_rmse(file, ref):
    p = Popen(['compare', '-metric', 'RMSE', file, ref, '.compare.png'],
              stdin=PIPE, stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()

    m = re.match(r'(\d+\.?\d*)', err)
    if m is None:
        rmse = 'ERROR: ' + output + err
    else:
        rmse = m.group(1)

    return rmse

def run_benchmark(app, setting, path, global_args, time_sec):
    results = ['', '']

    for alg in algorithms:
        print '   > running ' + alg + ' ... '

        for scheduling in scheduler_args:
            print '   > ' + scheduling['name']

            # Determine arguments and run imbatracer
            out_filename = path + setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_' + str(time_sec) + 'sec' + '.png'

            args = [app, setting['scene'],
                    '-w', str(setting['width']),
                    '-h', str(setting['height']),
                    '-q', '-t', str(time_sec), '-a', alg,
                    out_filename]
            args.extend(setting['args'])
            args.extend(global_args)
            args.extend(scheduling['args'])

            if convergence:
                args.extend(['--intermediate-path', path + setting['base_filename'] + '_' + alg + '_conv_',
                             '--intermediate-time', str(convergence_step_sec)])

            p = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)

            output, err = p.communicate()

            output_lines = output.splitlines()
            perf_result = output_lines[len(output_lines) - 1]

            print '   > ' + perf_result

            # Parse the results with regular expressions
            m = re.match(r'Done after (\d+\.?\d*) seconds, (\d+) samples @ (\d+\.?\d*) frames per second, (\d+\.?\d*)ms per frame', perf_result)

            time = m.group(1)
            samples = m.group(2)
            fps = m.group(3)
            ms_per_frame = m.group(4)

            rmse = compute_rmse(out_filename, setting['reference'])

            print '   > RMSE: ' + rmse
            print '   > '

            results[0] += setting['name'] + ',' + alg + ',' + time + ',' + samples + ',' + fps + ',' + ms_per_frame + ',' + rmse + ',' + scheduling['name'] + '\n'

            # Compute RMSE values for the image in the convergence test
            if convergence:
                results[1] += setting['name'] + ',' + alg + '\n'
                results[1] += 'time ms' + ',' + 'RMSE\n'
                for file in os.listdir(path):
                    beginning = setting['base_filename'] + '_' + alg + '_conv_'
                    m = re.match(beginning + r'(\d+)ms.png', file)
                    if m is None:
                        continue

                    time = m.group(1)
                    rmse = compute_rmse(path + file, setting['reference'])

                    results[1] += time + ',' + rmse + '\n'


        print '   > finished ' + alg
        print '   > ==================================='

    return results

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'Invalid command line arguments. Expected path to imbatracer executable.'
        quit()

    app = sys.argv[1]

    args = []
    if len(sys.argv) > 2:
        args = sys.argv[2:]

    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')

    for time_sec in times_in_seconds:
        res_file = open('results/result_' + timestamp + '_' + str(time_sec) + 'sec.csv', 'w')
        res_file.write('name, algorithm, time (seconds), samples, frames per second, ms per frame, RMSE, scheduling scheme\n')

        if convergence:
            conv_res_file = open('results/converge_' + timestamp + '_' + str(time_sec) + 'sec.csv', 'w')

        foldername = 'results/images_' + timestamp + '/' + str(time_sec) + 'sec'
        os.makedirs(foldername)

        i = 1
        for setting in bench_settings:
            print '== Running benchmark ' + str(i) + ' / ' + str(len(bench_settings)) + ' - ' + setting['name']
            bench_res = run_benchmark(app, setting, foldername + '/', args, time_sec)
            res_file.write(bench_res[0])
            conv_res_file.write(bench_res[1])

            res_file.flush()
            os.fsync(res_file.fileno())

            conv_res_file.flush()
            os.fsync(conv_res_file.fileno())

            i += 1
