const erizoFcTasks = (gulp, plugins, config) => {
  const that = {};
  if (!config.paths) {
    return;
  }
  const erizoFcConfig = {
    entry: `${config.paths.entry}ErizoFc.js`,
    webpackConfig: require('../webpack.config.erizofc.js'),
    debug: `${config.paths.debug}/erizofc`,
    production: `${config.paths.production}/erizofc`,
  };
  that.bundle = () =>
    gulp.src(erizoFcConfig.entry)
    .pipe(plugins.webpackGulp(erizoFcConfig.webpackConfig, plugins.webpack))
    .on('error', anError => console.log('An error ', anError))
    .pipe(gulp.dest(erizoFcConfig.debug))
    .on('error', anError => console.log('An error ', anError));

  that.compile = () => {
    return gulp.src(`${erizoFcConfig.debug}/**/*.js`)
    .pipe(gulp.dest(erizoFcConfig.production));
  }

  that.dist = () =>
    gulp.src(`${erizoFcConfig.production}/**/*.js`)
    .pipe(gulp.dest(config.paths.spine));

  that.clean = () =>
    plugins.del([`${erizoFcConfig.debug}/**/*.js*`, `${erizoFcConfig.production}/**/*.js*`],
    { force: true });

  return that;
};

module.exports = erizoFcTasks;
