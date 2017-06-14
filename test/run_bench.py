from subprocess import Popen, PIPE, call
import sys, os, shutil
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
    #     'args': []
    # },

    # {
    #     'name': 'Cornell specular balls',
    #     'scene': 'scenes/cornell/cornell_specular_front.scene',
    #     'reference': 'references/ref_cornell_specular_front.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_specular_front',
    #     'args': []
    # },

    # {
    #     'name': 'Cornell specular balls close',
    #     'scene': 'scenes/cornell/cornell_specular.scene',
    #     'reference': 'references/ref_cornell_specular.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_specular',
    #     'args': []
    # },

    # {
    #     'name': 'Cornell indirect',
    #     'scene': 'scenes/cornell/cornell_indirect.scene',
    #     'reference': 'references/ref_cornell_indirect.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_indirect',
    #     'args': []
    # },

    # {
    #     'name': 'Cornell water',
    #     'scene': 'scenes/cornell/cornell_water.scene',
    #     'reference': 'references/ref_cornell_water.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'cornell_water',
    #     'args': []
    # },

    {
        'name': 'Sponza behind curtain',
        'scene': 'scenes/sponza/sponza.scene',
        'reference': 'references/ref_sponza_curtain.png',
        'width': 1024,
        'height': 1024,
        'base_filename': 'sponza_curtain',
        'args': []
    },

    {
        'name': 'Still Life',
        'scene': 'scenes/stilllife/still_life.scene',
        'reference': 'references/ref_still_life.png',
        'width': 1280,
        'height': 720,
        'base_filename': 'still_life',
        'args': ['--max-path-len', '22']
    },

    # {
    #     'name': 'Car',
    #     'scene': 'scenes/car/car.scene',
    #     'reference': 'references/ref_car.png',
    #     'width': 1280,
    #     'height': 720,
    #     'base_filename': 'car',
    #     'args': ['--max-path-len', '22']
    # },
]


thread_counts   = [4]
sample_counts   = [1]
tilesizes       = [256]
connections     = [1]

scheduler_args = []
for t in thread_counts:
    for s in sample_counts:
        for tile in tilesizes:
            for c in connections:
                scheduler_args.append({
                    'name': 'tilesize: ' + str(tile) + ', threads: ' + str(t) + ', samples: ' + str(s) + ', connections: ' + str(c),
                    'abbr': str(tile) + str(t) + str(s) + str(c),
                    'args': ['-c', str(c), '--thread-count', str(t), '--tile-size', str(tile), '--spp', str(s)],
                    'samples_per_frame': s
                    })

times_in_seconds = [30]
same_time = True
algorithms = ['def_twpt', 'def_bpt']
convergence = False
convergence_step_sec = 5
light_path_frac = 0.5

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


def run_benchmark(app, setting, path, time_sec, cmd_args):
    results = ['', '']

    for alg in algorithms:
        print '   > running ' + alg + ' ... '

        for scheduling in scheduler_args:
            print '   > ' + scheduling['name']

            # Determine arguments and run imbatracer
            out_filename = path + setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_' + str(time_sec) + 'sec' + '.png'

            light_path_count = light_path_frac * setting['width'] * setting['height']
            print '   > ' + str(light_path_count) + ' light paths per frame'

            args = [app, setting['scene'],
                    '-w', str(setting['width']),
                    '-h', str(setting['height']),
                    '-q', '-a', alg,
                    '--light-path-count', str(light_path_count),
                    out_filename]
            args.extend(setting['args'])
            args.extend(scheduling['args'])

            if same_time:
                args.extend(['-t', str(time_sec)])
            else:
                args.extend(['-s', str(time_sec)])

            if convergence:
                args.extend(['--intermediate-path', path + setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_conv_',
                             '--intermediate-time', str(convergence_step_sec)])

            args += cmd_args

            p = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)

            output, err = p.communicate()

            output_lines = output.splitlines()

            if output_lines[len(output_lines) - 1][0] == "D":
                perf_result = output_lines[len(output_lines) - 1]
                ray_count = 0
            elif output_lines[len(output_lines) - 1][0] == "N" and output_lines[len(output_lines) - 2][0] == "D":
                perf_result = output_lines[len(output_lines) - 2]
                ray_count   = output_lines[len(output_lines) - 1]

                m = re.match(r'Number primary rays: (\d+) Number shadow rays: (\d+)', ray_count)
                if m is None:
                    print 'imbatracer failed. Output: \n' + output + err
                    continue

                ray_count = int(m.group(1)) + int(m.group(2))
            elif output_lines[len(output_lines) - 1][0] == "N" and output_lines[len(output_lines) - 2][0] == "N":
                perf_result = output_lines[len(output_lines) - 3]
                ray_count   = output_lines[len(output_lines) - 2]
                ray_count2  = output_lines[len(output_lines) - 1]

                m = re.match(r'Number primary rays: (\d+) Number shadow rays: (\d+)', ray_count)
                if m is None:
                    print 'imbatracer failed. Output: \n' + output + err
                    continue

                ray_count = int(m.group(1)) + int(m.group(2))

                m = re.match(r'Number primary rays: (\d+) Number shadow rays: (\d+)', ray_count2)
                if m is None:
                    print 'imbatracer failed. Output: \n' + output + err
                    continue

                ray_count2 = int(m.group(1)) + int(m.group(2))

                ray_count += ray_count2
            else:
                print "ERROR: last line of output: " + output_lines[len(output_lines) - 1]
                continue

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

            if not ray_count:
                rays_per_sec = '0'
            else:
                rays_per_sec = str(float(ray_count) / float(time));

            print '   > RMSE: ' + rmse
            print '   > Rays per second: ' + rays_per_sec
            print '   > '

            results[0] += setting['name'] + ',' + alg + ',' + time + ',' + samples + ',' + fps + ',' + ms_per_frame + ',' + rmse + ',' + rays_per_sec + ',' + scheduling['name'] + '\n'

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
                    '--spp', '8', '--tile-size', '128',
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


def generate_mis_images(app, setting, path, time_sec, cmd_args):
    results = ['', '']

    alg = 'vcm'
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
        args += cmd_args

        p = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        output, err = p.communicate()

        # Post-process the images
        call(["./convert_mis_images.sh"])

        # Move all images to the correct folder
        for f in os.listdir('.'):
            if f.endswith('.png'):
                shutil.move(f, path + setting['base_filename'] + '_' + alg + scheduling['abbr'] + '_' + str(time_sec) + 'sec' + f)


    print '   > finished ' + alg
    print '   > ==================================='

    return results


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'Invalid command line arguments. Expected path to imbatracer executable.'
        quit()

    benchmarker = run_benchmark

    app = sys.argv[1]

    args = []

    if len(sys.argv) >= 3:
        if sys.argv[2] == '-C':
            run_convergence_tests(app)
            quit()
        elif sys.argv[2] == '-c':
            run_convergence_tests(app)
        elif sys.argv[2] == '-w':
            benchmarker = generate_mis_images
        else:
            # All other arguments are forwarded to the renderer.
            args = [sys.argv[2]]

        args += sys.argv[3:]

    # Run benchmarks
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')

    for time_sec in times_in_seconds:
        res_file = open('results/result_' + timestamp + '_' + str(time_sec) + 'sec.csv', 'w')
        res_file.write('name, algorithm, time (seconds), samples, frames per second, ms per frame, RMSE, rays per second, scheduling scheme\n')

        if convergence:
            conv_res_file = open('results/converge_' + timestamp + '_' + str(time_sec) + 'sec.csv', 'w')

        foldername = 'results/images_' + timestamp + '/' + str(time_sec) + 'sec'
        os.makedirs(foldername)

        i = 1
        for setting in bench_settings:
            print '== Running benchmark ' + str(i) + ' / ' + str(len(bench_settings)) + ' - ' + setting['name']
            bench_res = benchmarker(app, setting, foldername + '/', time_sec, args)
            res_file.write(bench_res[0])

            if convergence:
                conv_res_file.write(bench_res[1])

            res_file.flush()
            os.fsync(res_file.fileno())

            if convergence:
                conv_res_file.flush()
                os.fsync(conv_res_file.fileno())

            i += 1
