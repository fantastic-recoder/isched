import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter, Router } from '@angular/router';
import { BootstrapPage } from './bootstrap.page';

describe('BootstrapPage', () => {
  let httpMock: HttpTestingController;
  let router: Router;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [BootstrapPage],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        provideRouter([{ path: 'login', component: BootstrapPage }]),
      ],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
    router = TestBed.inject(Router);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('navigates to login after successful bootstrap', (done) => {
    const fixture = TestBed.createComponent(BootstrapPage);
    const comp = fixture.componentInstance;
    const navSpy = jest.spyOn(router, 'navigate').mockResolvedValue(true);
    fixture.detectChanges();

    comp.form.setValue({
      email: 'admin@example.com',
      displayName: 'Admin',
      password: 'LongEnoughPassword123',
    });
    comp.submit();

    httpMock.expectOne('/graphql').flush({
      data: { bootstrapPlatformAdmin: { token: 'x', expiresAt: '2099-01-01T00:00:00Z' } },
    });

    Promise.resolve().then(() => {
      expect(navSpy).toHaveBeenCalledWith(['/login']);
      done();
    });
  });
});

