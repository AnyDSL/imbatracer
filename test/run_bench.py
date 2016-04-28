from subprocess import Popen, PIPE
import sys, os
import re
import datetime

# contains a dictionary of settings for every benchmark test
bench_settings = [
    # {
    #     'name': 'Cornell box',
    #     'scene': 'scenes/cornell/cornell_org.scene',
    #     'reference': 'references/ref_cornell_org.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell',
    #     'args': ['-r', '0.003']
    # },

    # {
    #     'name': 'Cornell specular balls',
    #     'scene': 'scenes/cornell/cornell_specular_front.scene',
    #     'reference': 'references/ref_cornell_specular_front.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_specular_front',
    #     'args': ['-r', '0.003']
    # },

    # {
    #     'name': 'Cornell specular balls close',
    #     'scene': 'scenes/cornell/cornell_specular.scene',
    #     'reference': 'references/ref_cornell_specular.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_specular',
    #     'args': ['-r', '0.003']
    # },

    # {
    #     'name': 'Cornell indirect',
    #     'scene': 'scenes/cornell/cornell_indirect.scene',
    #     'reference': 'references/ref_cornell_indirect.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_indirect',
    #     'args': ['-r', '0.003']
    # },

    # {
    #     'name': 'Cornell water',
    #     'scene': 'scenes/cornell/cornell_water.scene',
    #     'reference': 'references/ref_cornell_water.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_water',
    #     'args': ['-r', '0.003']
    # },

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
    #     'name': 'default: 256x256, 4 threads, 1 spp',
    #     'abbr': 'default',
    #     'args': []
    # },

    # {
    #     'name': 'default 4 spp: 256x256, 4 threads, 4 spp',
    #     'abbr': 'default_spp4',
    #     'args': ['--spp', '4']
    # },

    # {
    #     'name': 'default 8 spp: 256x256, 4 threads, 8 spp',
    #     'abbr': 'default_spp8',
    #     'args': ['--spp', '8']
    # },

    # ##############################################################
    # # Single threaded version for reference
    # {
    #     'name': 'single thread: 1024x1024, 1 threads, 1 spp',
    #     'abbr': 'single',
    #     'args': ['--thread-count', '1', '--tile-size', '1024']
    # },

    # {
    #     'name': 'single thread: 1024x1024, 1 threads, 4 spp',
    #     'abbr': 'single_4',
    #     'args': ['--thread-count', '1', '--tile-size', '1024', '--spp', '4']
    # },

    # {
    #     'name': 'single thread: 1024x1024, 1 threads, 8 spp',
    #     'abbr': 'single_8',
    #     'args': ['--thread-count', '1', '--tile-size', '1024', '--spp', '8']
    # },

    # {
    #     'name': 'single thread: 1024x1024, 1 threads, 16 spp',
    #     'abbr': 'single_16',
    #     'args': ['--thread-count', '1', '--tile-size', '1024', '--spp', '16']
    # },

    ##################################################################
    # 2 threads
    # {
    #     'name': '2 thread: 1024x1024, 2 threads, 1 spp',
    #     'abbr': 'dual_1',
    #     'args': ['--thread-count', '2', '--tile-size', '128']
    # },

    # {
    #     'name': '2 thread: 1024x1024, 2 threads, 4 spp',
    #     'abbr': 'dual_4',
    #     'args': ['--thread-count', '2', '--tile-size', '128', '--spp', '4']
    # },

    # {
    #     'name': '2 thread: 1024x1024, 2 threads, 8 spp',
    #     'abbr': 'dual_8',
    #     'args': ['--thread-count', '2', '--tile-size', '128', '--spp', '8']
    # },

    # {
    #     'name': '2 thread: 1024x1024, 2 threads, 16 spp',
    #     'abbr': 'dual_16',
    #     'args': ['--thread-count', '2', '--tile-size', '128', '--spp', '16']
    # },

    ##################################################################
    # 6 threads
    # {
    #     'name': '6 thread: 1024x1024, 6 threads, 1 spp',
    #     'abbr': 'hecta_1',
    #     'args': ['--thread-count', '6', '--tile-size', '128']
    # },

    # {
    #     'name': '6 thread: 1024x1024, 6 threads, 4 spp',
    #     'abbr': 'hecta_4',
    #     'args': ['--thread-count', '6', '--tile-size', '128', '--spp', '4']
    # },

    # {
    #     'name': '6 thread: 1024x1024, 6 threads, 8 spp',
    #     'abbr': 'hecta_8',
    #     'args': ['--thread-count', '6', '--tile-size', '128', '--spp', '8']
    # },

    # {
    #     'name': '6 thread: 1024x1024, 6 threads, 16 spp',
    #     'abbr': 'hecta_16',
    #     'args': ['--thread-count', '6', '--tile-size', '128', '--spp', '16']
    # },

    ##################################################################
    # 8 threads
    # {
    #     'name': '8 thread: 1024x1024, 8 threads, 1 spp',
    #     'abbr': 'octa_1',
    #     'args': ['--thread-count', '8', '--tile-size', '128']
    # },

    # {
    #     'name': '8 thread: 1024x1024, 8 threads, 4 spp',
    #     'abbr': 'octa_4',
    #     'args': ['--thread-count', '8', '--tile-size', '128', '--spp', '4']
    # },

    # {
    #     'name': '8 thread: 1024x1024, 8 threads, 8 spp',
    #     'abbr': 'octa_8',
    #     'args': ['--thread-count', '8', '--tile-size', '128', '--spp', '8']
    # },

    # {
    #     'name': '8 thread: 1024x1024, 8 threads, 16 spp',
    #     'abbr': 'octa_16',
    #     'args': ['--thread-count', '8', '--tile-size', '128', '--spp', '16']
    # },

    ##############################################################
    # 4 threads
    # {
    #     'name': 'smaller tiles: 128x128, 4 threads ,1 spp',
    #     'abbr': 'quad',
    #     'args': ['--tile-size', '128']
    # },

    # {
    #     'name': 'smaller tiles 4 spp: 128x128, 4 threads, 4 spp',
    #     'abbr': 'quad_spp4',
    #     'args': ['--spp', '4', '--tile-size', '128']
    # },

    # {
    #     'name': 'smaller tiles 8 spp: 128x128, 4 threads, 8 spp',
    #     'abbr': 'quad_spp8',
    #     'args': ['--spp', '8', '--tile-size', '128']
    # },

    # {
    #     'name': 'smaller tiles 8 spp: 128x128, 4 threads, 16 spp',
    #     'abbr': 'quad_spp16',
    #     'args': ['--spp', '16', '--tile-size', '128']
    # },
]

all_algorithms = ['pt', 'bpt', 'vcm', 'lt', 'ppm']

times_in_seconds = [60] #[5, 10, 30, 60]
algorithms = ['pt', 'bpt', 'vcm']
convergence = False
convergence_step_sec = 5


def compute_rmse(file, ref):
    p = Popen(['compare', '-metric', 'RMSE', file, ref, '.compare.png'],
              stdin=PIPE, stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()

    m = re.match(r'(\d+\.?\d*)', err)
    if m is None:
        rmse = 'ERROR: ' + output + err
    else:
        rmse = m.group(1)

    try:
        os.remove('.compare.png')
    except:
        pass

    return rmse


def run_benchmark(app, setting, path, time_sec):
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
            args.extend(scheduling['args'])

            if convergence:
                args.extend(['--intermediate-path', path + setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_conv_',
                             '--intermediate-time', str(convergence_step_sec)])

            p = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)

            output, err = p.communicate()

            output_lines = output.splitlines()
            perf_result = output_lines[len(output_lines) - 1]

            print '   > ' + perf_result

            # Parse the results with regular expressions
            m = re.match(r'Done after (\d+\.?\d*) seconds, (\d+) samples @ (\d+\.?\d*) frames per second, (\d+\.?\d*)ms per frame', perf_result)

            if m is None:
                print 'imbatracer failed. Output: \n' + output + err
                continue

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
                results[1] += 'time s' + ',' + 'RMSE\n'
                for file in os.listdir(path):
                    filename_base = setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_conv_'
                    m = re.match(filename_base + r'(\d+)ms.png', file)
                    if m is None:
                        continue

                    time = float(m.group(1)) / 1000.0
                    rmse_step = compute_rmse(path + file, setting['reference'])

                    results[1] += str(time) + ',' + rmse_step + '\n'
                results[1] += str(time_sec) + ',' + rmse + '\n'


        print '   > finished ' + alg
        print '   > ==================================='

    return results


def run_convergence_tests(app):
    """ Test to check if all algorithms eventually converge to the given reference image.

    Uses only the path tracer, bi-directinal path tracer and vertex connection and merging, because
    the other algorithms are biased and cannot reproduce some effects.
    """
    output_dir = 'results/convergence'
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print 'Running convergence tests...'

    convergence_algs = ['pt', 'bpt', 'vcm']
    for setting in bench_settings:
        print setting['name']

        convergence_algs = ['pt', 'bpt', 'vcm']
        for alg in convergence_algs:
            print alg

            out_filename = output_dir + '/' + setting['base_filename'] + alg + '.png'

            args = [app, setting['scene'],
                    '-w', str(setting['width']),
                    '-h', str(setting['height']),
                    '-q', '-t', '3600', '-a', alg,
                    '--spp', '4',
                    out_filename]
            args.extend(setting['args'])

            p = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
            output, err = p.communicate()

            if not os.path.isfile(out_filename):
                print 'ERROR: File was not created. Output: ' + output + err
                continue

            # compute RMSE
            rmse = compute_rmse(out_filename, setting['reference'])
            print ' > RMSE: ' + rmse

    print 'DONE'
    print ''


def run_scaling_bench(app, setting, path, time_sec):
    pass


if __name__ == '__main__':
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print 'Invalid command line arguments. Expected path to imbatracer executable.'
        quit()

    app = sys.argv[1]

    args = []
    if len(sys.argv) == 3:
        if sys.argv[2] != '-c':
            print 'Unknown argument: ' + sys.argv[2]
        else:
            run_convergence_tests(app)

    # Run benchmarks
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
            bench_res = run_benchmark(app, setting, foldername + '/', time_sec)
            res_file.write(bench_res[0])

            if convergence:
                conv_res_file.write(bench_res[1])

            res_file.flush()
            os.fsync(res_file.fileno())

            if convergence:
                conv_res_file.flush()
                os.fsync(conv_res_file.fileno())

            i += 1
