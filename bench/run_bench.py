from subprocess import Popen, PIPE
import sys
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
        'args': []
    },

    # {
    #     'name': 'Sibenik',
    #     'scene': 'scenes/sibenik/sibenik.scene',
    #     'reference': 'references/ref_sibenik.png',
    #     'width': 1024,
    #     'height': 1024,
    #     'base_filename': 'sibenik',
    #     'args': []
    # }
]

alg_small = ['pt', 'bpt', 'vcm']
alg_large = ['pt', 'bpt', 'vcm', 'lt', 'ppm']
alg_pt_only = ['pt']

time_sec = 5
algorithms = alg_small

def run_benchmark(app, setting, path):
    results = ''

    for alg in algorithms:
        print '   > running ' + alg + ' ... '

        out_filename = path + setting['base_filename'] + '_' + alg + '.png'

        args = [app, setting['scene'],
                '-w', str(setting['width']),
                '-h', str(setting['height']),
                '-q', '-t', str(time_sec), '-a', alg,
                out_filename]
        args.extend(setting['args'])

        p = Popen(args,
                  stdin=PIPE, stdout=PIPE, stderr=PIPE)

        output, err = p.communicate()

        output_lines = output.splitlines()
        perf_result = output_lines[len(output_lines) - 1]

        print '   > ' + perf_result

        m = re.match(r'Done after (\d+\.\d*) seconds, (\d+) samples @ (\d+\.\d*) frames per second, (\d+\.?\d*)ms per frame', perf_result)

        time = m.group(1)
        samples = m.group(2)
        fps = m.group(3)
        ms_per_frame = m.group(4)

        # Compute RMSE with ImageMagick
        p = Popen(['compare', '-metric', 'RMSE', out_filename, setting['reference'], '.compare.png'],
                  stdin=PIPE, stdout=PIPE, stderr=PIPE)
        output, err = p.communicate()

        m = re.match(r'(\d+\.\d*)', err)
        if m is None:
            rmse = 'ERROR: ' + output + err
        else:
            rmse = m.group(1)

        print '   > RMSE: ' + rmse
        print '   > '

        results += setting['name'] + ',' + alg + ',' + time + ',' + samples + ',' + fps + ',' + ms_per_frame + ',' + rmse + '\n'

    return results

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Invalid command line arguments. Expected path to imbatracer executable.'
        quit()

    app = sys.argv[1]

    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    res_file = open('results/result_' + timestamp + '.csv', 'w')
    res_file.write('name, algorithm, time (seconds), samples, frames per second, ms per frame, RMSE\n')

    i = 1
    for setting in bench_settings:
        print '== Running benchmark ' + str(i) + ' / ' + str(len(bench_settings)) + ' - ' + setting['name']
        res_file.write(run_benchmark(app, setting, 'results/images_' + timestamp + '/'))
        i += 1
